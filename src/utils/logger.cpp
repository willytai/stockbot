
#include "logger.h"
#include "spdlog/async.h"
#include "spdlog/async_logger.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace stockbot {

const std::string PATTERN("%^[%Y-%m-%d %X] [%L] [%n] %v%$");

std::shared_ptr<spdlog::logger> Logger::__logger;

void Logger::init(spdlog::level::level_enum logLevel) {
    spdlog::init_thread_pool(8192, 2);
    std::vector<spdlog::sink_ptr> sinks = {
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>("stockbot.log", 1024 * 1024 * 5, 10),
    };
    __logger = std::make_shared<spdlog::async_logger>(
        "stockbot",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
    __logger->set_pattern(PATTERN);
    __logger->set_level(logLevel);
    __logger->debug("Logger '{}' Initialized.", __logger->name());
    __logger->flush_on(spdlog::level::debug);

    spdlog::register_logger(__logger);
}

std::shared_ptr<spdlog::logger> Logger::createWithSharedSinksAndLevel(const std::string& name)
{
    if (!__logger) return nullptr;

    auto sinks = __logger->sinks();
    auto logger = std::make_shared<spdlog::async_logger>(
        name,
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
    logger->set_pattern(PATTERN);
    logger->set_level(__logger->level());
    logger->debug("Logger '{}' Initialized.", logger->name());
    logger->flush_on(spdlog::level::debug);

    spdlog::register_logger(logger);

    return logger;
}

}
