#include "app.h"
#include "discordBot.h"
#include "investmentManager.h"
#include "utils/logger.h"
#include "nlohmann/json.hpp"
#include <fstream>

namespace stockbot {

using json = nlohmann::json;

static spdlog::level::level_enum to_spdlog_log_level(App::LogLevel level)
{
    switch(level) {
        case App::LogLevel::Debug: return spdlog::level::debug;
        case App::LogLevel::Trace: return spdlog::level::trace;
    }
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
        investmentManagerLogger
    );

    // set the flag
    m_shouldRun = true;

    // start the discrod bot (this is async)
    m_discordBot->run();

    // connect schwab client (this is sync)
    if (m_schwabClient->connect()) {
        // keep a copy of the linked accounts
        m_linkedAccounts = m_schwabClient->getLinkedAccounts();

        // // TEST: testing some calls here
        // m_schwabClient->startStreamer();
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
    }


    // block
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_shouldRun; });

    // release these
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

schwabcpp::AccountsSummaryMap
App::getAccountSummary()
{
    return m_schwabClient ? m_schwabClient->accountSummary() : schwabcpp::AccountsSummaryMap{};
}

}
