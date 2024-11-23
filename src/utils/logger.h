#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "spdlog/spdlog.h"
#include <memory>

namespace stockbot {

class Logger
{
public:
    Logger() = default;
    ~Logger() = default;

    // default turns off trace
    static void init(spdlog::level::level_enum logLevel = spdlog::level::debug);

    static std::shared_ptr<spdlog::logger> createWithSharedSinksAndLevel(const std::string& name, std::shared_ptr<spdlog::logger> logger = nullptr);

    static inline std::shared_ptr<spdlog::logger> getLogger() { return __logger; }

private:
    static std::shared_ptr<spdlog::logger> __logger;
};


}

#ifndef TARGET_LOGGER
#define TARGET_LOGGER ::stockbot::Logger::getLogger()
#endif

#define LOG_TRACE(...) TARGET_LOGGER->trace(__VA_ARGS__)
#define LOG_DEBUG(...) TARGET_LOGGER->debug(__VA_ARGS__)
#define LOG_WARN(...)  TARGET_LOGGER->warn(__VA_ARGS__)
#define LOG_INFO(...)  TARGET_LOGGER->info(__VA_ARGS__)
#define LOG_ERROR(...) TARGET_LOGGER->error(__VA_ARGS__)
#define LOG_FATAL(...) TARGET_LOGGER->critical(__VA_ARGS__); exit(1);

#define VERIFY(condition, ...) \
    if (!condition) { \
        LOG_WARN(__VA_ARGS__); \
    }

#define NOTIMPLEMENTEDERROR LOG_WARN("Not Implemented: {} @ {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__)

#endif /* ifndef __LOGGER_H__ */
