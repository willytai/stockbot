#include "discordBot.h"
#include "app.h"
#include "autoInvestment.h"
#include "command/command.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "nlohmann/json.hpp"
#include "schwabcpp/client.h"

// dpp
#include "cluster.h"
#include "colors.h"
#include "once.h"

// I want all the logs uses DiscordBot::m_botLogger
#ifdef TARGET_LOGGER
#undef TARGET_LOGGER
#endif
#define TARGET_LOGGER m_botLogger

namespace stockbot {

namespace {

static const std::string INVESTMENT_SETUP_FORM_ID("recurring_investment_form");
static const std::string INVESTMENT_SETUP_FORM_TICKER_FIELD("field_ticker");
static const std::string INVESTMENT_SETUP_FORM_SHARES_FIELD("field_shares");
static const std::string INVESTMENT_SETUP_FORM_FREQUENCY_FIELD("field_frequency");
static const std::string INVESTMENT_SETUP_FORM_TRIGGER_THRESHOLD_FIELD("field_triggerThreshold");
static const std::string INVESTMENT_SETUP_FORM_SKIP_THRESHOLD_FIELD("field_skipThreshold");

}

using json = nlohmann::json;
using Timer = schwabcpp::Timer;

DiscordBot::DiscordBot(const std::string& token,
                       const std::string& adminUserId,
                       bool reregisterCommands,
                       std::shared_ptr<App> app,
                       std::shared_ptr<spdlog::logger> logger)
    : m_reregisterCommands(reregisterCommands)
    , m_authRequired(false)
    , m_token(token)
    , m_adminUserId(adminUserId)
    , m_botLogger(logger)
    , m_app(app)
{
    // create a shared sink logger for the discord bot
    m_dppLogger = Logger::createWithSharedSinksAndLevel("dpp", m_botLogger);
    // always debug level for this (trace shows too many stuffs)
    m_dppLogger->set_level(spdlog::level::debug);

    // configure the discord bot
    m_dbot = std::make_unique<dpp::cluster>(m_token);
    m_dbot->on_log(std::bind(&DiscordBot::onLog, this, std::placeholders::_1));
    m_dbot->on_slashcommand(std::bind(&DiscordBot::onSlashCommand, this, std::placeholders::_1));
    m_dbot->on_form_submit(std::bind(&DiscordBot::onFormSubmit, this, std::placeholders::_1));
    m_dbot->on_ready(std::bind(&DiscordBot::onReady, this, std::placeholders::_1));

    LOG_INFO("Discord bot initialized.");
}

DiscordBot::~DiscordBot()
{
    stop();
}

void DiscordBot::run()
{
    // start dpp, returns immediately
    m_dbot->start(dpp::st_return);

    LOG_INFO("Discord bot started.");
}

void DiscordBot::stop()
{
    if (m_dbot) {
        LOG_INFO("Stopping discord bot..");
        m_dbot.reset();
    }
}

// -- Schwab client callbacks

void DiscordBot::onSchwabClientEvent(schwabcpp::Event& event)
{
    schwabcpp::EventDispatcher dispatcher(event);

    dispatcher.dispatch<schwabcpp::OAuthUrlRequestEvent>(std::bind(&DiscordBot::onSchwabClientOAuthUrlRequest, this, std::placeholders::_1));
    dispatcher.dispatch<schwabcpp::OAuthCompleteEvent>(std::bind(&DiscordBot::onSchwabClientOAuthComplete, this, std::placeholders::_1));
}

bool DiscordBot::onSchwabClientOAuthUrlRequest(schwabcpp::OAuthUrlRequestEvent& event)
{
    // set flag
    m_authRequired = true;

    std::string desc;
    switch (event.getReason()) {
        case schwabcpp::OAuthUrlRequestEvent::Reason::InitialSetup: {
            desc = "To start the schwab client, please follow the link and authorize.\n"
                   "Send the redirected url with the /" + command::Authorize::Name() + " command after authorization.";
            break;
        }
        case schwabcpp::OAuthUrlRequestEvent::Reason::RefreshTokenExpired: {
            desc = "The refresh token has expired.\n"
                   "Please follow the link, reauthorize, and use the /" + command::Authorize::Name() + " command to send the redirected url to continue service.";
            break;
        }
        case schwabcpp::OAuthUrlRequestEvent::Reason::PreviousAuthFailed: {
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
        .set_url(event.getAuthorizationUrl())
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

    // wait for the user to use the slash comamnd to authorize
    std::unique_lock lock(m_mutexOAuth);
    m_OAuthRedirectedUrl.clear();
    m_cvOAuth.wait(lock, [this]{ return !m_OAuthRedirectedUrl.empty(); });

    // reply to the event to send the url back
    event.reply(m_OAuthRedirectedUrl);

    return true;  // if we reach this point, the event is handled
}

bool DiscordBot::onSchwabClientOAuthComplete(schwabcpp::OAuthCompleteEvent& event)
{
    // set flag
    // note that this just means we are out of the auth flow
    // it doesn't mean that auth succeeded
    m_authRequired = false;

    dpp::embed embed = dpp::embed()
        .set_timestamp(time(0));

    switch (event.getStatus()) {
        case schwabcpp::OAuthCompleteEvent::Status::Succeeded: {
            embed
                .set_color(dpp::colors::green)
                .set_title("OAuth Successful")
                .set_description("Schwab client is online.");
            break;
        }
        case schwabcpp::OAuthCompleteEvent::Status::Failed: {
            // TODO: add a flow to restart client when this happens
            embed
                .set_color(dpp::colors::red)
                .set_title("OAuth Failed")
                .set_description("Schwab client terminated.");
            break;
        }
        case schwabcpp::OAuthCompleteEvent::Status::NotRequired: {
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

    return true;  // always handled
}

// -- DPP callbacks

void DiscordBot::onLog(const dpp::log_t& event)
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

void DiscordBot::onReady(const dpp::ready_t& event)
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
            m_dbot->global_bulk_command_create(commands, [this](dpp::confirmation_callback_t confirmation) {
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

void DiscordBot::onSlashCommand(const dpp::slashcommand_t& event)
{
    SlashCommandDispatcher dispatcher(event);

    dispatcher.dispatch<command::Ping>(std::bind(&DiscordBot::onPingEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::Kill>(std::bind(&DiscordBot::onKillEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::Authorize>(std::bind(&DiscordBot::onAuthorizeEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::AllAccountInfo>(std::bind(&DiscordBot::onAllAccountInfoEvent, this, std::placeholders::_1));
    dispatcher.dispatch<command::SetupRecurringInvestment>(std::bind(&DiscordBot::onSetupRecurringInvestmentEvent, this, std::placeholders::_1));
}

void DiscordBot::onFormSubmit(const dpp::form_submit_t& event)
{
    if (event.custom_id == INVESTMENT_SETUP_FORM_ID) {
        LOG_INFO("Recurring investment form received from user id: {}.", event.command.usr.id.str());

        AutoInvestment investment;

        // for holding the error messages
        std::vector<std::string> errors;

        // extract the data
        // event.components is a vector of the row components
        // event.components[i] is the components at row i
        // event.components[i].components[j] is the jth component at the ith row
        // the form can only have 1 component each row, so we are only intereseted in event.components[i].components[0]
        for (const dpp::component& component: event.components) {
            const dpp::component& comp = component.components[0];

            if (comp.custom_id == INVESTMENT_SETUP_FORM_TICKER_FIELD) {
                investment.ticker = std::get<std::string>(comp.value);
                utils::strip(investment.ticker);
                utils::toUpper(investment.ticker);
            } else if (comp.custom_id == INVESTMENT_SETUP_FORM_SHARES_FIELD) {
                try {
                    // split the field value by "+"
                    std::vector<std::string> list;
                    utils::split(std::get<std::string>(comp.value), list, "+");

                    // first one is the shares
                    investment.shares = std::stoi(list[0]);

                    // second one is the extras (optional)
                    if (list.size() > 1) {
                        investment.extras = std::stoi(list[1]);
                    }
                } catch (const std::exception& e) {
                    errors.push_back("Unable to extract 'Number of Shares' field. Expected format: <shares>[+<extras>]. (Error: " + std::string(e.what()) + ")");
                } catch (...) {
                    LOG_ERROR("Unhandled error for {}", INVESTMENT_SETUP_FORM_SHARES_FIELD);
                }
            } else if (comp.custom_id == INVESTMENT_SETUP_FORM_FREQUENCY_FIELD) {
                try {
                    std::string freqString = std::get<std::string>(comp.value);
                    utils::strip(freqString);
                    utils::toLower(freqString);
                    // set the frequency
                    if (freqString == "daily") {
                        investment.frequency = AutoInvestment::Daily;
                    } else if (freqString == "weekly") {
                        investment.frequency = AutoInvestment::Weekly;
                    } else {
                        throw std::invalid_argument("Unrecognized frequency string.");
                    }
                } catch (const std::exception& e) {
                    errors.push_back("Unable to extract 'Frequency' field. Expected format: <daily|weekly>. (Error: " + std::string(e.what()) + ")");
                } catch (...) {
                    LOG_ERROR("Unhandled error for {}", INVESTMENT_SETUP_FORM_FREQUENCY_FIELD);
                }
            } else if (comp.custom_id == INVESTMENT_SETUP_FORM_TRIGGER_THRESHOLD_FIELD) {
                try {
                    std::string triggerThresholdString = std::get<std::string>(comp.value);
                    investment.averageInThreshold = utils::parseAsPercentage(triggerThresholdString);
                } catch (const std::exception& e) {
                    errors.push_back("Unable to extract 'Trigger Threshold' field. Expected format: <number>[%]. (Error: " + std::string(e.what()) + ")");
                } catch (...) {
                    LOG_ERROR("Unhandled error for {}", INVESTMENT_SETUP_FORM_TRIGGER_THRESHOLD_FIELD);
                }
            } else if (comp.custom_id == INVESTMENT_SETUP_FORM_SKIP_THRESHOLD_FIELD) {
                try {
                    std::string skipThresholdString = std::get<std::string>(comp.value);
                    investment.skipThreshold = utils::parseAsPercentage(skipThresholdString);
                } catch (const std::exception& e) {
                    errors.push_back("Unable to extract 'Skip Threshold' field. Expected format: <number>[%]. (Error: " + std::string(e.what()) + ")");
                } catch (...) {
                    LOG_ERROR("Unhandled error for {}", INVESTMENT_SETUP_FORM_SKIP_THRESHOLD_FIELD);
                }
            } else {
                LOG_ERROR("Unrecognized form field: {}", comp.custom_id);
            }
        }

        // continue if we are good
        if (errors.empty()) {
            // reset time stamp
            investment.createdTime = clock::now().time_since_epoch().count();
            investment.lastTriggerTime = 0;

            LOG_DEBUG("Created auto investment: \n{}", json(investment).dump(4));

            event.reply(dpp::message("Form received.").set_flags(dpp::m_ephemeral));

            // callback into the app
            m_app->registerAutoInvestment(investment);
        } else {
            // log the errors
            std::string desc;
            for (const std::string& error : errors) {
                LOG_WARN("{}", error);
                desc += "> _" + error + "_\n\n";
            }

            // notify the user
            // TODO: add a resubmit button?
            dpp::embed embed = dpp::embed()
                .set_color(dpp::colors::red)
                .set_title("Unable to Setup Investment")
                .set_description(desc)
                .set_timestamp(time(0));
            event.reply(dpp::message(event.command.channel_id, embed).set_flags(dpp::m_ephemeral));
        }
    } else {
        LOG_WARN("Received an unrecognized form, id: {}", event.custom_id);
    }
}

// -- Slash command callbacks

#define SLASH_COMMAND_TRACE(name, event) LOG_INFO("'{}' requested from user: {}", name, event.command.usr.format_username());

void DiscordBot::onPingEvent(const dpp::slashcommand_t& event)
{
    SLASH_COMMAND_TRACE(command::Ping::Name(), event);

    event.reply("HI");
}

void DiscordBot::onKillEvent(const dpp::slashcommand_t& event)
{
    SLASH_COMMAND_TRACE(command::Kill::Name(), event);
    
    event.reply(dpp::message("Stopping the app...").set_flags(dpp::m_ephemeral), [this](auto) {
        // stop the app after the message is sent
        m_app->stop();
    });
}

void DiscordBot::onAuthorizeEvent(const dpp::slashcommand_t& event)
{
    SLASH_COMMAND_TRACE(command::Authorize::Name(), event);

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

void DiscordBot::onAllAccountInfoEvent(const dpp::slashcommand_t& event)
{
    SLASH_COMMAND_TRACE(command::AllAccountInfo::Name(), event);

    schwabcpp::AccountsSummaryMap summaryMap = m_app->getAccountSummary();
    
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

void DiscordBot::onSetupRecurringInvestmentEvent(const dpp::slashcommand_t& event)
{
    SLASH_COMMAND_TRACE(command::SetupRecurringInvestment::Name(), event);

    dpp::interaction_modal_response modal(INVESTMENT_SETUP_FORM_ID, "Recurring Investment Setup");

    modal.add_component(
        dpp::component()
            .set_label("Ticker")
            .set_id(INVESTMENT_SETUP_FORM_TICKER_FIELD)
            .set_type(dpp::cot_text)
            .set_placeholder("NVDA")
            .set_text_style(dpp::text_short)
    );

    modal.add_row();
    modal.add_component(
        dpp::component()
            .set_label("Number of Shares")
            .set_id(INVESTMENT_SETUP_FORM_SHARES_FIELD)
            .set_type(dpp::cot_text)
            .set_placeholder("5")
            .set_text_style(dpp::text_short)
    );

    modal.add_row();
    modal.add_component(
        dpp::component()
            .set_label("Frequency")
            .set_id(INVESTMENT_SETUP_FORM_FREQUENCY_FIELD)
            .set_type(dpp::cot_selectmenu)
            .set_placeholder("Daily")
            .set_text_style(dpp::text_short)
    );

    modal.add_row();
    modal.add_component(
        dpp::component()
            .set_label("Trigger Threshold")
            .set_id(INVESTMENT_SETUP_FORM_TRIGGER_THRESHOLD_FIELD)
            .set_type(dpp::cot_text)
            .set_placeholder("2%")
            .set_text_style(dpp::text_short)
    );

    modal.add_row();
    modal.add_component(
        dpp::component()
            .set_label("Skip Threshold")
            .set_id(INVESTMENT_SETUP_FORM_SKIP_THRESHOLD_FIELD)
            .set_type(dpp::cot_text)
            .set_placeholder("1%")
            .set_text_style(dpp::text_short)
    );

    event.dialog(modal);
}


}
