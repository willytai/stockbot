#include "stockBot.h"
#include "utils/logger.h"

int main(int argc, char *argv[]) {

    {
        stockbot::Bot bot(stockbot::Bot::LogLevel::Trace);
        bot.run();
    }

    LOG_INFO("Program exited normally.");

    return 0;
}
