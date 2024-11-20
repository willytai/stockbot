#include "stockBot.h"
#include "utils/logger.h"

int main(int argc, char *argv[])
{
    bool reregisterCommands = argc > 1 &&
                              !strcmp(argv[1], "reregisterCommands");

    {
        stockbot::Bot bot({
            .logLevel = stockbot::Bot::LogLevel::Trace,
            .reregisterCommands = reregisterCommands,
        });
        bot.run();
    }

    LOG_INFO("Program exited normally.");

    return 0;
}
