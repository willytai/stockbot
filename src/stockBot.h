#include <filesystem>
#include <memory>
#include "dpp/dispatcher.h"

namespace dpp {
class cluster;
}

namespace schwabcpp {
class Client;
}

namespace stockbot {

class Bot
{
public:
    enum class LogLevel : char {
        Debug,
        Trace
    };
                                        Bot();
                                        Bot(LogLevel level);
                                        Bot(const std::filesystem::path& appCredentialPath);
                                        Bot(const std::filesystem::path& appCredentialPath, LogLevel level);
                                        ~Bot();

    void                                run();

private:
    void                                init(const std::filesystem::path& appCredentialPath, LogLevel level);
    void                                stop();

    // -- Event handlers for the discord bot
    void                                onDiscordBotReady(const dpp::ready_t& event);

private:
    std::unique_ptr<dpp::cluster>       m_dbot;  // the discord bot
    std::unique_ptr<schwabcpp::Client>  m_client;
    std::string                         m_token;

    std::mutex                          m_mutex;
    std::condition_variable             m_cv;
    bool                                m_shouldRun;
};

}
