#include "autoInvestment.h"
#include "schwabcpp/event/eventBase.h"
#include "schwabcpp/schema/accountSummary.h"
#include <filesystem>
#include <condition_variable>

namespace schwabcpp {
class Client;
}

namespace stockbot {

class DiscordBot;
class InvestmentManager;
class TaskManager;

class App : public std::enable_shared_from_this<App>
{
    struct AccountInfo {
        std::string accountNumber;
        std::string displayAcctId;
        std::string nickName;
        bool        primaryAccount = false;
    };
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

    // -- Accessors
    const std::vector<AccountInfo>&     getLinkedAccounts() const { return m_linkedAccounts; }

private:
    // -- Schwab client callbacks
    void                                onSchwabClientEvent(schwabcpp::Event& event);
    void                                streamerDataHandler(const std::string& data);

private:
    // -- APIs for the discord bot to call
    friend class DiscordBot;
    schwabcpp::AccountsSummaryMap       getAccountSummary();
    void                                addPendingAutoInvestment(AutoInvestment&& investment);
    void                                linkAndRegisterAutoInvestment(const std::string& investmentId, const std::vector<std::string>& accounts);
    void                                stop();

private:
    // -- APIs for the investment manager to call
    friend class InvestmentManager;
    void                                subscribeTickersToStream(const std::vector<std::string>& tickers);
    void                                registerTask(std::function<void()> task);

private:
    // -- Convenience helpers
    bool                                isMarketOpen() const;

private:
    std::unique_ptr<DiscordBot>         m_discordBot;
    std::unique_ptr<schwabcpp::Client>  m_schwabClient;
    std::unique_ptr<InvestmentManager>  m_investmentManager;
    std::unique_ptr<TaskManager>        m_taskManager;

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

    // -- Linked Accounts
    std::vector<AccountInfo>            m_linkedAccounts;
};

}
