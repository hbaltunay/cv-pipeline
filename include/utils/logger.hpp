#pragma once

#ifdef STACK_ANALYSIS
    #define LOG_TRACE(msg, ...)    std::puts("[TRACE] " msg)
    #define LOG_DEBUG(msg, ...)    std::puts("[DEBUG] " msg)
    #define LOG_INFO(msg, ...)     std::puts("[INFO] " msg)
    #define LOG_WARN(msg, ...)     std::puts("[WARN] " msg)
    #define LOG_ERROR(msg, ...)    std::puts("[ERROR] " msg)
    #define LOG_CRITICAL(msg, ...) std::puts("[CRIT] " msg)
#else
    #ifndef SPDLOG_ACTIVE_LEVEL
        #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
    #endif

    #include <spdlog/spdlog.h>

    #define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
    #define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
    #define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
    #define LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
    #define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
    #define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
#endif

namespace Logger {
    void init();
}