#include "stockBot.h"
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
    m_dbot->on_slashcommand([this](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping") {
            event.reply("Pong!");
        } else if (event.command.get_command_name() == "kill") {
            event.reply("Killing the bot..");
            {
                std::lock_guard lock(m_mutex);
                m_shouldRun = false;
            }
            m_cv.notify_all();
        }
    });
    m_dbot->on_ready(
        std::bind(&Bot::onDiscordBotReady, this, std::placeholders::_1)
    );
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
    if (dpp::run_once<struct register_bot_commands>()) {
    
        dpp::slashcommand ping("ping", "Ping pong!", m_dbot->me.id);
        dpp::slashcommand kill("kill", "Kills the bot!", m_dbot->me.id);
        kill.set_default_permissions(dpp::p_administrator);
        m_dbot->global_bulk_command_create({
            ping,
            kill
        });
    }
}

}
