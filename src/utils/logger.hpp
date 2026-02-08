#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <string_view>

namespace video_bench {

class Logger {
public:
    static std::string defaultLogFilePath();
    static bool initialize(const std::string& log_file_path, std::string& error_message);
    static void info(std::string_view message);
    static void error(std::string_view message);
    static void shutdown();

private:
    Logger() = delete;
};

} // namespace video_bench

#endif // LOGGER_HPP
