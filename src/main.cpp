#include "app.h"
#include "utils/logger.h"

#include "boost/stacktrace.hpp"
#include <signal.h>
#include <iostream>
#include <sys/signal.h>

std::thread::id MAIN_THREAD_ID;

void onSignal(int sig)
{
    ::signal(sig, SIG_DFL);
    std::cerr << "=== thread " << std::this_thread::get_id();
    if (MAIN_THREAD_ID == std::this_thread::get_id()) {
        std::cerr << " (main thread)";
    }
    std::cerr << std::endl;
    std::cerr << boost::stacktrace::stacktrace();
    ::raise(SIGABRT);
}

int main(int argc, char *argv[])
{
    MAIN_THREAD_ID = std::this_thread::get_id();

    // register signal handlers for more verbose messages
    ::signal(SIGSEGV, &onSignal);
    ::signal(SIGBUS, &onSignal);

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
