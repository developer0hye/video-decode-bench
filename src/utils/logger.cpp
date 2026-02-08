#include "utils/logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <mutex>

namespace video_bench {
namespace {

constexpr const char* kLoggerName = "video_benchmark";

std::shared_ptr<spdlog::logger> g_logger;
std::mutex g_logger_mutex;

std::shared_ptr<spdlog::logger> getLogger() {
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    return g_logger;
}

} // namespace

std::string Logger::defaultLogFilePath() {
    return "video-benchmark.log";
}

bool Logger::initialize(const std::string& log_file_path, std::string& error_message) {
    std::lock_guard<std::mutex> lock(g_logger_mutex);

    if (g_logger) {
        return true;
    }

    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
        file_sink->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");

        g_logger = std::make_shared<spdlog::logger>(kLoggerName, file_sink);
        g_logger->set_level(spdlog::level::info);
        g_logger->flush_on(spdlog::level::info);
        spdlog::register_logger(g_logger);
        return true;
    } catch (const spdlog::spdlog_ex& ex) {
        error_message = ex.what();
        g_logger.reset();
        return false;
    }
}

void Logger::info(std::string_view message) {
    auto logger = getLogger();
    if (!logger) {
        return;
    }
    logger->info("{}", message);
}

void Logger::error(std::string_view message) {
    auto logger = getLogger();
    if (!logger) {
        return;
    }
    logger->error("{}", message);
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(g_logger_mutex);

    if (!g_logger) {
        return;
    }

    g_logger->flush();
    spdlog::drop(kLoggerName);
    g_logger.reset();
}

} // namespace video_bench
