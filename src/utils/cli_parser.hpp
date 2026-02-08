#ifndef CLI_PARSER_HPP
#define CLI_PARSER_HPP

#include "benchmark/benchmark_config.hpp"
#include <string>
#include <optional>
#include <vector>

namespace video_bench {

struct CliParseResult {
    bool success;
    bool show_help;
    bool show_version;
    BenchmarkConfig config;
    std::string error_message;
};

class CliParser {
public:
    static CliParseResult parse(int argc, char* argv[]);

    static void printUsage(const std::string& program_name);
    static void printVersion();

private:
    static std::optional<int> parseInteger(const std::string& str);
    static std::optional<double> parseDouble(const std::string& str);
};

} // namespace video_bench

#endif // CLI_PARSER_HPP
