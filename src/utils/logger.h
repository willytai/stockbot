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
    static inline std::shared_ptr<spdlog::logger> getLogger() { return __logger; }

private:
    static std::shared_ptr<spdlog::logger> __logger;
};


}

#define LOG_TRACE(...) ::stockbot::Logger::getLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::stockbot::Logger::getLogger()->debug(__VA_ARGS__)
#define LOG_WARN(...)  ::stockbot::Logger::getLogger()->warn(__VA_ARGS__)
#define LOG_INFO(...)  ::stockbot::Logger::getLogger()->info(__VA_ARGS__)
#define LOG_ERROR(...) ::stockbot::Logger::getLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...) ::stockbot::Logger::getLogger()->critical(__VA_ARGS__); exit(1);

#define VERIFY(condition, ...) \
    if (!condition) { \
        LOG_WARN(__VA_ARGS__); \
    }

#define NOTIMPLEMENTEDERROR LOG_WARN("Not Implemented: {} @ {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__)

#endif /* ifndef __LOGGER_H__ */
