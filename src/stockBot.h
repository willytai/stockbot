#include <filesystem>
#include <memory>
#include "dpp/dispatcher.h"
#include "schwabcpp/client.h"

namespace dpp {
class cluster;
}

namespace spdlog {
class logger;
}

namespace stockbot {

class Bot
{
public:
    enum class LogLevel : char {
        Debug,
        Trace,
    };

    struct Spec {
        std::filesystem::path   appCredentialPath = "./.appCredentials.json";
        LogLevel                logLevel = LogLevel::Debug;
        bool                    reregisterCommands = false;
    };
                                        Bot(const Spec& spec);
                                        ~Bot();

    void                                run();

private:
    void                                stop();

    // -- OAuth callbacks for the client
    std::string                         onSchwabOAuthUrlRequest(
                                            const std::string& url,
                                            schwabcpp::Client::AuthRequestReason requestReason,
                                            int chancesLeft);
    void                                onSchwabOAuthComplete(
                                            schwabcpp::Client::AuthStatus status
                                        );

    // -- Event handlers for the discord bot
    void                                onDiscordBotLog(const dpp::log_t& event);
    void                                onDiscordBotReady(const dpp::ready_t& event);
    void                                onDiscordBotSlashCommand(const dpp::slashcommand_t& event);

    // -- Slash command handlers
    void                                onPingEvent(const dpp::slashcommand_t& event);
    void                                onKillEvent(const dpp::slashcommand_t& event);
    void                                onAuthorizeEvent(const dpp::slashcommand_t& event);
    void                                onAllAccountInfoEvent(const dpp::slashcommand_t& event);
    void                                onSetupRecurringInvestmentEvent(const dpp::slashcommand_t& event);

private:
    std::unique_ptr<dpp::cluster>       m_dbot;                 // the discord bot
    std::string                         m_token;                // bot token
    dpp::snowflake                      m_adminUserId;          // admin: YOU!
    bool                                m_reregisterCommands;   // reregister commands on startup
    std::unique_ptr<schwabcpp::Client>  m_client;               // schwap client

    // -- State Management
    std::mutex                          m_mutex;
    std::condition_variable             m_cv;
    bool                                m_shouldRun;

    // -- Logging for dpp
    std::shared_ptr<spdlog::logger>     m_dppLogger;

    // -- CV to block the OAuth callback and wait for admin to send back the OAuth redirected url
    std::mutex                          m_mutexOAuth;
    std::condition_variable             m_cvOAuth;
    std::string                         m_OAuthRedirectedUrl;
};

}
