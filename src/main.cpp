#include "app.h"
#include "utils/logger.h"

int main(int argc, char *argv[])
{
    bool reregisterCommands = argc > 1 &&
                              !strcmp(argv[1], "reregisterCommands");

    {
        std::shared_ptr<stockbot::App> app = std::make_shared<stockbot::App>(stockbot::App::Spec{
            .logLevel = stockbot::App::LogLevel::Trace,
            .reregisterDiscordBotSlashCommands = reregisterCommands,
        });
        app->run();
    }

    LOG_INFO("Program exited normally.");

    return 0;
}
