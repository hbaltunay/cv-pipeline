#include "logger.hpp"

#ifdef STACK_ANALYSIS
namespace Logger {
    void init() {}
}
#else

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>
#include <mutex>
#include <vector>
#include <memory>

namespace Logger {
    void init() {
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            if (!std::filesystem::exists("logs")) {
                std::filesystem::create_directories("logs");
            }

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "logs/pipeline.log", 1024 * 1024 * 10, 5);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
            
            auto logger = std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());

            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%3!l%$] [%t] %v");
            logger->set_level(spdlog::level::trace);
            logger->flush_on(spdlog::level::warn);

            spdlog::set_default_logger(logger);

            LOG_INFO("Logger has been configured.");
        });
    }
}
#endif