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

class App;

class DiscordBot
{
public:
                                        DiscordBot(
                                            const std::string& token,
                                            const std::string& adminUserId,
                                            bool reregisterCommands,
                                            std::shared_ptr<App> app,
                                            std::shared_ptr<spdlog::logger> logger
                                        );
                                        ~DiscordBot();

    void                                run();

    void                                onSchwabClientEvent(schwabcpp::Event& event);

private:
    void                                stop();

    // -- Event handlers for the schwab client
    bool                                onSchwabClientOAuthUrlRequest(schwabcpp::OAuthUrlRequestEvent& event);
    bool                                onSchwabClientOAuthComplete(schwabcpp::OAuthCompleteEvent& event);

    // -- Event handlers for the discord bot
    void                                onLog(const dpp::log_t& event);
    void                                onReady(const dpp::ready_t& event);
    void                                onSlashCommand(const dpp::slashcommand_t& event);

    // -- Slash command handlers
    void                                onPingEvent(const dpp::slashcommand_t& event);
    void                                onKillEvent(const dpp::slashcommand_t& event);
    void                                onAuthorizeEvent(const dpp::slashcommand_t& event);
    void                                onAllAccountInfoEvent(const dpp::slashcommand_t& event);
    void                                onSetupRecurringInvestmentEvent(const dpp::slashcommand_t& event);

private:
    std::shared_ptr<App>                m_app;                  // keeps a reference to the app
    std::unique_ptr<dpp::cluster>       m_dbot;                 // the discord bot
    std::string                         m_token;                // bot token
    dpp::snowflake                      m_adminUserId;          // admin: YOU!
    bool                                m_reregisterCommands;   // reregister commands on startup

    // -- Loggers
    std::shared_ptr<spdlog::logger>     m_dppLogger;            // for dpp
    std::shared_ptr<spdlog::logger>     m_botLogger;            // for ourself

    // -- CV to block the OAuth callback and wait for admin to send back the OAuth redirected url
    //    (not elegant but I can't figure out how to use the dpp sync APIs...)
    std::mutex                          m_mutexOAuth;
    std::condition_variable             m_cvOAuth;
    std::string                         m_OAuthRedirectedUrl;
    std::atomic<bool>                   m_authRequired;  // so that we can ignore authorize command when it is not required
};

}
