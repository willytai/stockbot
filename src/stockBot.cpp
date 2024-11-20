#include "stockBot.h"
#include "command/command.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "utils/logger.h"
#include "nlohmann/json.hpp"
#include "dpp/dpp.h"
#include "schwabcpp/client.h"
#include <fstream>

namespace stockbot {

using json = nlohmann::json;
using Timer = schwabcpp::Timer;

namespace {

static spdlog::level::level_enum to_spdlog_log_level(Bot::LogLevel level)
{
    switch(level) {
        case Bot::LogLevel::Debug: return spdlog::level::debug;
        case Bot::LogLevel::Trace: return spdlog::level::trace;
    }
}

}

Bot::Bot(const Spec& spec)
    : m_shouldRun(false)
    , m_reregisterCommands(spec.reregisterCommands)
    , m_authRequired(false)
{
    // logger for stockbot
    Logger::init(to_spdlog_log_level(spec.logLevel));

    LOG_INFO("Initializing bot..");

    // load credentials
    std::ifstream file(spec.appCredentialPath);
    if (file.is_open()) {
        json credentialData;
        file >> credentialData;

        // bot token
        if (credentialData.contains("bot_token")) {
            m_token = credentialData["bot_token"];
        } else {
            LOG_FATAL("Discord bot token missing in credential file!!");
        }

        // admin info (this is the user the bot will be reporting to)
        if (credentialData.contains("admin_user_id")) {
            m_adminUserId = credentialData["admin_user_id"].get<std::string>();
        } else {
            LOG_FATAL("Admin user id missing in credential file!!");
        }
    } else {
        LOG_FATAL("Unable to open the app credentials file: {}", spec.appCredentialPath.string());
    }
}

Bot::~Bot()
{
    stop();
}

void Bot::run()
{
    // set the flag
    m_shouldRun = true;

    // create a shared sink logger for the discord bot
    m_dppLogger = Logger::createWithSharedSinksAndLevel("dpp");
    // always debug level for this (trace shows too many stuffs)
    m_dppLogger->set_level(spdlog::level::debug);

    // create and start the discord bot
    m_dbot = std::make_unique<dpp::cluster>(m_token);
    m_dbot->on_log(std::bind(&Bot::onDiscordBotLog, this, std::placeholders::_1));
    m_dbot->on_slashcommand(std::bind(&Bot::onDiscordBotSlashCommand, this, std::placeholders::_1));
    m_dbot->on_ready(std::bind(&Bot::onDiscordBotReady, this, std::placeholders::_1));
    m_dbot->start(dpp::st_return);

    LOG_INFO("Discord bot started.");

    // start the schwab client
    // create a shared sink logger for client
    m_client = std::make_unique<schwabcpp::Client>(schwabcpp::Client::Spec{
        .oAuthUrlRequestCallback = std::bind(
            &Bot::onSchwabOAuthUrlRequest,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        ),
        .oAuthCompleteCallback = std::bind(
            &Bot::onSchwabOAuthComplete,
            this,
            std::placeholders::_1
        ),
        .logger = Logger::createWithSharedSinksAndLevel("schwabcpp"),
    });
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

std::string Bot::onSchwabOAuthUrlRequest(const std::string& url,
                                         schwabcpp::Client::AuthRequestReason requestReason,
                                         int chancesLeft)
{
    // set flag
    m_authRequired = true;

    std::string desc;
    switch (requestReason) {
        case schwabcpp::Client::AuthRequestReason::InitialSetup: {
            desc = "To start the schwab client, please follow the link and authorize.\n"
                   "Send the redirected url with the /" + command::Authorize::Name() + " command after authorization.";
            break;
        }
        case schwabcpp::Client::AuthRequestReason::RefreshTokenExpired: {
            desc = "The refresh token has expired.\n"
                   "Please follow the link, reauthorize, and use the /" + command::Authorize::Name() + " command to send the redirected url to continue service.";
            break;
        }
        case schwabcpp::Client::AuthRequestReason::PreviousAuthFailed: {
            desc = "Failed to generate new tokens.\n"
                   "The redirected url expires rather fast. Make sure you send it within 30 seconds.\n"
                   "Please follow the link and reauthorize.";
            break;
        }
    }

    // send the notification for reauthorization
    dpp::embed embed = dpp::embed()
        .set_color(dpp::colors::brass)
        .set_title("Schwab Client OAuth Request")
        .set_url(url)
        .set_description(desc)
        .set_timestamp(time(0));

    // send the dm to admin
    m_dbot->direct_message_create(m_adminUserId, dpp::message(embed), [this](dpp::confirmation_callback_t confirmation){
        if (confirmation.is_error()) {
            LOG_ERROR("Could not send dm to admin for OAuth request. Error: {}", confirmation.get_error().human_readable);
        } else {
            LOG_INFO("OAuth request sent to admin user.");
        }
    });

    // wait for command oauth redirected url
    std::unique_lock lock(m_mutexOAuth);
    m_OAuthRedirectedUrl.clear();
    m_cvOAuth.wait(lock, [this]{ return !m_OAuthRedirectedUrl.empty(); });

    // send it back
    return m_OAuthRedirectedUrl;
}

void Bot::onSchwabOAuthComplete(schwabcpp::Client::AuthStatus status)
{
    // set flag
    // note that this just means we are out of the auth flow
    // it doesn't mean that auth succeeded
    m_authRequired = false;

    dpp::embed embed = dpp::embed()
        .set_timestamp(time(0));

    switch (status) {
        case schwabcpp::Client::AuthStatus::Succeeded: {
            embed
                .set_color(dpp::colors::green)
                .set_title("OAuth Successful")
                .set_description("Schwab client is online.");
            break;
        }
        case schwabcpp::Client::AuthStatus::Failed: {
            // TODO: add a flow to restart client when this happens
            embed
                .set_color(dpp::colors::red)
                .set_title("OAuth Failed")
                .set_description("Schwab client terminated.");
            break;
        }
        case schwabcpp::Client::AuthStatus::NotRequired: {
            embed
                .set_color(dpp::colors::green)
                .set_title("OAuth Not Required")
                .set_description("Schwab client is online.");
            break;
        }
    }

    // send the dm to admin
    m_dbot->direct_message_create(m_adminUserId, dpp::message(embed), [this](dpp::confirmation_callback_t confirmation){
        if (confirmation.is_error()) {
            LOG_ERROR("Could not send dm to admin for OAuth status. Error: {}", confirmation.get_error().human_readable);
        } else {
            LOG_INFO("OAuth status sent to admin user.");
        }
    });
}

void Bot::onDiscordBotLog(const dpp::log_t& event)
{
    switch (event.severity) {
        case dpp::ll_trace:
            m_dppLogger->trace("{}", event.message);
            break;
        case dpp::ll_debug:
            m_dppLogger->debug("{}", event.message);
            break;
        case dpp::ll_info:
            m_dppLogger->info("{}", event.message);
            break;
        case dpp::ll_warning:
            m_dppLogger->warn("{}", event.message);
            break;
        case dpp::ll_error:
            m_dppLogger->error("{}", event.message);
            break;
        case dpp::ll_critical:
        default:
            m_dppLogger->critical("{}", event.message);
            break;
    }
}

void Bot::onDiscordBotReady(const dpp::ready_t& event)
{
    if (dpp::run_once<struct register_bot_commands>()) {

        // we might reconnect so putting this message in run once to show once
        LOG_INFO("Discord bot is up and ready.");

        // register commands ONLY IF reregister is requested
        if (m_reregisterCommands) {

            // usage:
            // dpp::slashcommand command(<command name>, <command description>, m_dbot->me.id);

            auto id = m_dbot->me.id;

            std::vector<dpp::slashcommand> commands;

            {
                dpp::slashcommand command(command::Ping::Name(), "Ping pong!", id);
                commands.push_back(command);
            }

            {
                dpp::slashcommand command(command::Kill::Name(), "Kills the bot!", id);
                command.set_default_permissions(dpp::p_administrator);
                commands.push_back(command);
            }

            {
                dpp::slashcommand command(command::AllAccountInfo::Name(), "Displays the information of all accounts.", id);
                commands.push_back(command);
            }

            {
                dpp::slashcommand command(command::SetupRecurringInvestment::Name(), "Setup a recurring investment.", id);
                commands.push_back(command);
            }

            {
                dpp::slashcommand command(command::Authorize::Name(), "Authorize schwab client with the redirected url.", id);
                command.set_default_permissions(dpp::p_administrator);
                command.add_option(dpp::command_option(dpp::co_string, "redirect_url", "Final redirected url after authorization.", true));
                commands.push_back(command);
            }

            // dpp makes command unavailable in dms by default
            // turning it on for global commands
            std::for_each(commands.begin(), commands.end(), [](dpp::slashcommand& cmd){
                cmd.set_dm_permission(true);
            });
            m_dbot->global_bulk_command_create(commands, [](dpp::confirmation_callback_t confirmation) {
                if (confirmation.is_error()) {
                    LOG_ERROR(
                        "Unable to create global bulk commands. Error: {} ({}).",
                        confirmation.get_error().human_readable,
                        confirmation.get_error().code
                    );
                } else {
                    LOG_INFO("Global bulk commands registered.");
                }
            });
        }
    }
}

void Bot::onDiscordBotSlashCommand(const dpp::slashcommand_t& event)
{
    SlashCommandDispatcher dispatcher(event);

    dispatcher.dispatch<command::Ping>(std::bind(&Bot::onPingEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::Kill>(std::bind(&Bot::onKillEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::Authorize>(std::bind(&Bot::onAuthorizeEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::AllAccountInfo>(std::bind(&Bot::onAllAccountInfoEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::SetupRecurringInvestment>(std::bind(&Bot::onSetupRecurringInvestmentEvent, this, std::placeholders::_1));
}

// -- Slash command callbacks

void Bot::onPingEvent(const dpp::slashcommand_t& event)
{
    event.reply("HI");
}

void Bot::onKillEvent(const dpp::slashcommand_t& event)
{
    LOG_INFO("Killing the bot...");
    
    event.reply(dpp::message("Killing the bot..").set_flags(dpp::m_ephemeral));
    {
        std::lock_guard lock(m_mutex);
        m_shouldRun = false;
    }
    m_cv.notify_all();
}

void Bot::onAuthorizeEvent(const dpp::slashcommand_t& event)
{
    // this command is for the admin user, i.e. YOU! only
    if (event.command.usr.id == m_adminUserId) {
        // proceed if in auth flow
        if (m_authRequired) {
            // retrieve the url
            std::string url = std::get<std::string>(event.get_parameter("redirect_url"));
            {
                std::lock_guard lock(m_mutexOAuth);
                m_OAuthRedirectedUrl = url;
            }
        
            LOG_INFO("Received redirected url: {}, notifying schwab client...", url);
        
            // notify the callback
            m_cvOAuth.notify_one();
        
            event.reply(dpp::message("Authorizing schwab client...").set_flags(dpp::m_ephemeral));
        } else {
            // ignore if not
            event.reply(dpp::message("Authorization not required at this time.").set_flags(dpp::m_ephemeral));
        }
    } else {
        LOG_WARN("A non-admin user {} tried to use the {} command.", event.command.usr.global_name, command::Authorize::Name());
    
        event.reply(dpp::message("YOU DON'T HAVE THE PERMISSION TO RUN THIS COMMAND!").set_flags(dpp::m_ephemeral));
    }
}

void Bot::onAllAccountInfoEvent(const dpp::slashcommand_t& event)
{
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
    
    event.reply(dpp::message(event.command.channel_id, embed).set_flags(dpp::m_ephemeral));
}

void Bot::onSetupRecurringInvestmentEvent(const dpp::slashcommand_t& event)
{
    // TODO:
    dpp::embed embed = dpp::embed()
        .set_color(dpp::colors::red)
        .set_title("Recurring Investment Setup")
        .set_timestamp(time(0))
        .add_field("THIS IS CURRENTLY UNDER DEVELOPMENT!!", "---");
    
    event.reply(dpp::message(event.command.channel_id, embed).set_flags(dpp::m_ephemeral));
}


}
