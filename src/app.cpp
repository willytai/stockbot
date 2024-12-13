#include "app.h"
#include "discordBot.h"
#include "investmentManager.h"
#include "taskManager.h"
#include "utils/logger.h"
#include "nlohmann/json.hpp"
#include <fstream>

namespace stockbot {

using json = nlohmann::json;

static spdlog::level::level_enum to_spdlog_log_level(App::LogLevel level)
{
    switch (level) {
        case App::LogLevel::Debug: return spdlog::level::debug;
        case App::LogLevel::Trace: return spdlog::level::trace;
    }

    return spdlog::level::debug;
}

App::App(const Spec& spec)
    : m_shouldRun(false)
    , m_reregisterDiscordBotSlashCommands(spec.reregisterDiscordBotSlashCommands)
{
    // init logger
    Logger::init(to_spdlog_log_level(spec.logLevel));

    // and load credentials
    if (std::filesystem::exists(spec.appCredentialPath)) {
        std::ifstream file(spec.appCredentialPath);
        if (file.is_open()) {
            json credentialData;
            file >> credentialData;

            // discord bot token
            if (credentialData.contains("bot_token")) {
                m_discordBotToken = credentialData["bot_token"];
            } else {
                LOG_FATAL("Discord bot token missing in credential file!!");
            }

            // admin info (this is the user the discord bot will be reporting to)
            if (credentialData.contains("admin_user_id")) {
                m_discrodBotAdminUserId = credentialData["admin_user_id"].get<std::string>();
            } else {
                LOG_FATAL("Admin user id for discord bot missing in credential file!!");
            }

            if (credentialData.contains("app_key") &&
                credentialData.contains("app_secret")) {
                m_schwabKey = credentialData["app_key"];
                m_schwabSecret = credentialData["app_secret"];
            } else {
                LOG_FATAL("Schwab client credentials missing!!");
            }
        } else {
            LOG_FATAL("Unable to open the app credentials file: {}", spec.appCredentialPath.string());
        }
    } else {
        LOG_FATAL("App credentials file: {} not found. Did you specify the right path?", spec.appCredentialPath.string());
    }

    LOG_INFO("App initialized.");
}

App::~App()
{
    stop();
}

void App::run()
{
    // create loggers with same sink but different tags for the discord bot, schwab client, and investment manager
    std::shared_ptr<spdlog::logger> discordBotLogger = Logger::createWithSharedSinksAndLevel("DiscordBot");
    std::shared_ptr<spdlog::logger> schwabClientLogger = Logger::createWithSharedSinksAndLevel("SchwabClient");
    std::shared_ptr<spdlog::logger> investmentManagerLogger = Logger::createWithSharedSinksAndLevel("InvestmentManager");
    std::shared_ptr<spdlog::logger> taskManagerLogger = Logger::createWithSharedSinksAndLevel("TaskManager");

    // discord bot
    m_discordBot = std::make_unique<DiscordBot>(
        m_discordBotToken,
        m_discrodBotAdminUserId,
        m_reregisterDiscordBotSlashCommands,
        shared_from_this(),
        discordBotLogger
    );

    // schwab client
    m_schwabClient = std::make_unique<schwabcpp::Client>(
        m_schwabKey,
        m_schwabSecret,
        schwabClientLogger
    );
    // custom callback for the schwab client
    m_schwabClient->setEventCallback(std::bind(&App::onSchwabClientEvent, shared_from_this(), std::placeholders::_1));

    // investment manager
    m_investmentManager = std::make_unique<InvestmentManager>(
        shared_from_this(),
        investmentManagerLogger
    );

    // task manager
    m_taskManager = std::make_unique<TaskManager>(
        2,  // pool size
        taskManagerLogger
    );

    // set the flag
    m_shouldRun = true;

    // start the discrod bot (this is async)
    m_discordBot->run();

    // connect schwab client (this is sync)
    if (m_schwabClient->connect()) {
        // keep a copy of some account info
        schwabcpp::UserPreference userPreference = m_schwabClient->getUserPreference();
        for (const schwabcpp::UserPreference::Account& account : userPreference.accounts) {
            m_linkedAccounts.push_back({
                .accountNumber = account.accountNumber,
                .displayAcctId = account.displayAcctId,
                .nickName = account.nickName,
                .primaryAccount = account.primaryAccount,
            });
        }

        // the streamer is constructed after a successful connection, the data handler has to be set here to take effect
        m_schwabClient->setStreamerDataHandler(std::bind(&App::streamerDataHandler, shared_from_this(), std::placeholders::_1));

        // start the streamer
        // TODO: maybe start the streamer after the first sub request arrives
        //       If you start the streamer with no subscriptions, it's gonna disconnect automatically after a few seconds.
        m_schwabClient->startStreamer();

        // // TEST: testing some calls here
        //
        // std::this_thread::sleep_for(std::chrono::seconds(10));
        //
        // // TEST: testing pause resume
        // {
        //     std::this_thread::sleep_for(std::chrono::seconds(5));
        //     m_schwabClient->pauseStreamer();
        //     std::this_thread::sleep_for(std::chrono::seconds(5));
        //     m_schwabClient->resumeStreamer();
        // }
        //
        // std::this_thread::sleep_for(std::chrono::seconds(10));
        //
        // m_schwabClient->stopStreamer();

        // start the investment manager
        m_investmentManager->run();

        // start the task manager
        m_taskManager->run();
    }


    // block
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_shouldRun; });

    // release these
    m_taskManager.reset();
    m_investmentManager.reset();
    m_schwabClient.reset();
    m_discordBot.reset();
}

void App::stop()
{
    // notify the run function to exit
    {
        std::lock_guard lock(m_mutex);
        m_shouldRun = false;
    }
    m_cv.notify_all();
}

void App::addPendingAutoInvestment(AutoInvestment&& investment)
{
    m_investmentManager->addPendingInvestment(std::move(investment));
}

void App::linkAndRegisterAutoInvestment(const std::string& investmentId, const std::vector<std::string>& accounts)
{
    m_investmentManager->linkAndRegisterAutoInvestment(investmentId, accounts);
}

void App::onSchwabClientEvent(schwabcpp::Event& event)
{
    // asking the discord bot to handle them first
    m_discordBot->onSchwabClientEvent(event);

    // maybe the app can do some default handling?
    if (!event.getHandled()) {
        // TODO:
        NOTIMPLEMENTEDERROR;
    }
}

void App::streamerDataHandler(const std::string& data)
{
    if (isMarketOpen()) {
        if (!data.empty()) {
            m_investmentManager->enqueueStreamData(data);
        } else {
            LOG_DEBUG("Empty stream data.");
        }
    } else {
        LOG_DEBUG("Market closed.");
    }
}

schwabcpp::AccountsSummaryMap
App::getAccountSummary()
{
    return m_schwabClient ? m_schwabClient->accountSummary() : schwabcpp::AccountsSummaryMap{};
}

void App::subscribeTickersToStream(const std::vector<std::string>& tickers)
{
    m_schwabClient->subscribeLevelOneEquities(
        tickers,
        {
            schwabcpp::StreamerField::LevelOneEquity::HighPrice,
            schwabcpp::StreamerField::LevelOneEquity::LowPrice,
            schwabcpp::StreamerField::LevelOneEquity::LastPrice,
            schwabcpp::StreamerField::LevelOneEquity::NetPercentChange,
        }
    );
}

void App::registerTask(std::function<void()> task)
{
    m_taskManager->addTask(task);
}

bool App::isMarketOpen() const
{
    return true;
    using clock = schwabcpp::clock;

    // now
    auto now = clock::now();

    // Use a specific time zone
    auto tz = std::chrono::locate_zone("America/New_York");
    auto localTime = std::chrono::zoned_time{tz, now}.get_local_time();
    auto timeInDays = std::chrono::floor<std::chrono::days>(localTime);
    auto timeOfDay = std::chrono::floor<std::chrono::seconds>(localTime - timeInDays);

    // Extract hour and minute
    auto localHMS = std::chrono::hh_mm_ss<std::chrono::seconds>(timeOfDay);
    int currentHour = localHMS.hours().count();
    int currentMinute = localHMS.minutes().count();

    // Get the day of the week (0 = Monday, ..., 6 = Sunday)
    auto weekday = std::chrono::weekday{timeInDays};

    // Market is open from 9:30 AM to 4:00 PM ET on weekdays
    return (currentHour > 9 || (currentHour == 9 && currentMinute >= 30))
        && (currentHour < 16)
        && (weekday != std::chrono::Saturday && weekday != std::chrono::Sunday);
}

}
