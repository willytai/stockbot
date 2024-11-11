#include "stockBot.h"
#include "command.h"
#include "utils/logger.h"
#include "nlohmann/json.hpp"
#include "dpp/dpp.h"
#include "schwabcpp/client.h"
#include <fstream>

namespace stockbot {

using json = nlohmann::json;

namespace {

static spdlog::level::level_enum to_spdlog_log_level(Bot::LogLevel level)
{
    switch(level) {
        case Bot::LogLevel::Debug: return spdlog::level::debug;
        case Bot::LogLevel::Trace: return spdlog::level::trace;
    }
}

}

Bot::Bot()
    : m_shouldRun(false)
{
    init("./.appCredentials.json", LogLevel::Debug);
}

Bot::Bot(LogLevel level)
    : m_shouldRun(false)
{
    init("./.appCredentials.json", level);
}

Bot::Bot(const std::filesystem::path& appCredentialPath)
    : m_shouldRun(false)
{
    init(appCredentialPath, LogLevel::Debug);
}

Bot::Bot(const std::filesystem::path& appCredentialPath, LogLevel level)
    : m_shouldRun(false)
{
    init(appCredentialPath, level);
}

Bot::~Bot()
{
    stop();
}

void Bot::init(const std::filesystem::path& appCredentialPath, LogLevel level)
{
    Logger::init(to_spdlog_log_level(level));

    LOG_INFO("Initializing bot..");

    // load credentials
    std::ifstream file(appCredentialPath);
    if (file.is_open()) {
        json credentialData;
        file >> credentialData;

        if (credentialData.contains("bot_token")) {
            m_token = credentialData["bot_token"];
        } else {
            LOG_FATAL("Discord bot token missing!!");
        }
    } else {
        LOG_FATAL("Unable to open the app credentials file: {}", appCredentialPath.string());
    }
}

void Bot::run()
{
    // set the flag
    m_shouldRun = true;

    // create and start the discord bot
    m_dbot = std::make_unique<dpp::cluster>(m_token);
    m_dbot->on_log(dpp::utility::cout_logger());
    m_dbot->on_slashcommand(std::bind(&Bot::onDiscordBotSlashCommand, this, std::placeholders::_1));
    m_dbot->on_ready(std::bind(&Bot::onDiscordBotReady, this, std::placeholders::_1));
    m_dbot->start(dpp::st_return);

    LOG_INFO("Discord bot started.");

    // start the schwab client
    m_client = std::make_unique<schwabcpp::Client>(schwabcpp::Client::LogLevel::Trace);
    m_client->startStreamer();

    std::this_thread::sleep_for(std::chrono::seconds(10));

    // TEST: testing pause resume
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        m_client->pauseStreamer();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        m_client->resumeStreamer();
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    m_client->stopStreamer();

    // block
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_shouldRun; });

    stop();
}

void Bot::stop()
{
    if (m_client) {
        m_client.reset();
    }

    if (m_dbot) {
        LOG_INFO("Killing discord bot..");
        m_dbot.reset();
    }
}

void Bot::onDiscordBotReady(const dpp::ready_t& event)
{
    // register commands
    if (dpp::run_once<struct register_bot_commands>()) {
    
        // usage:
        // dpp::slashcommand ping(<command name>, <command description>, m_dbot->me.id);

        auto id = m_dbot->me.id;

        std::vector<dpp::slashcommand> commands;

        {
            dpp::slashcommand command(Command::PING, "Ping pong!", id);
            commands.push_back(command);
        }

        {
            dpp::slashcommand command(Command::KILL, "Kills the bot!", id);
            command.set_default_permissions(dpp::p_administrator);
            commands.push_back(command);
        }

        {
            dpp::slashcommand command(Command::ALL_ACCOUNT_INFO, "Displays the information of all accounts.", id);
            commands.push_back(command);
        }

        m_dbot->global_bulk_command_create(commands);
    }
}

void Bot::onDiscordBotSlashCommand(const dpp::slashcommand_t& event)
{
    if (event.command.get_command_name() == Command::PING) {
        event.reply("Pong!");
    } else if (event.command.get_command_name() == Command::KILL) {
        event.reply("Killing the bot..");
        {
            std::lock_guard lock(m_mutex);
            m_shouldRun = false;
        }
        m_cv.notify_all();
    } else if (event.command.get_command_name() == Command::ALL_ACCOUNT_INFO) {
        schwabcpp::AccountsSummaryMap summaryMap = m_client->accountSummary();

        dpp::embed embed = dpp::embed()
            .set_color(dpp::colors::cyan)
            .set_title("Accounts Summary")
            .set_url("https://client.schwab.com/clientapps/accounts/summary/")
            .set_timestamp(time(0));

        for (const auto& [accountNumber, info] : summaryMap.summary) {
            embed = embed.add_field(
                "Account " + accountNumber,
                "value: " + std::to_string(info.aggregatedBalance.currentLiquidationValue) + "\n" +
                "cash:  " + std::to_string(info.securitiesAccount.currentBalances.cashAvailableForTrading)

            );
        }


        event.reply(dpp::message(event.command.channel_id, embed));
    }
}

}
