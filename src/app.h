#include "autoInvestment.h"
#include "schwabcpp/event/eventBase.h"
#include "schwabcpp/schema/accountSummary.h"
#include <filesystem>

namespace schwabcpp {
class Client;
}

namespace stockbot {

class DiscordBot;
class InvestmentManager;

class App : public std::enable_shared_from_this<App>
{
public:
    enum class LogLevel : char {
        Debug,
        Trace,
    };

    struct Spec {
        std::filesystem::path   appCredentialPath = "./.appCredentials.json";
        LogLevel                logLevel = LogLevel::Debug;
        bool                    reregisterDiscordBotSlashCommands = false;
    };

    App(const Spec& spec);
    virtual ~App();

    void                                run();

private:
    // -- Schwab client callback
    void                                onSchwabClientEvent(schwabcpp::Event& event);

private:
    // -- APIs for the discord bot to call
    friend class DiscordBot;
    schwabcpp::AccountsSummaryMap       getAccountSummary();
    void                                registerAutoInvestment(const AutoInvestment& investment);
    void                                stop();

private:
    std::unique_ptr<DiscordBot>         m_discordBot;
    std::unique_ptr<schwabcpp::Client>  m_schwabClient;
    std::unique_ptr<InvestmentManager>  m_investmentManager;

    // -- State Management
    std::mutex                          m_mutex;
    std::condition_variable             m_cv;
    bool                                m_shouldRun;

    // -- Credentials and Settings
    std::string                         m_discordBotToken;
    std::string                         m_discrodBotAdminUserId;
    std::string                         m_schwabKey;
    std::string                         m_schwabSecret;
    bool                                m_reregisterDiscordBotSlashCommands;
};

}
