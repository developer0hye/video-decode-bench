#ifndef OUTPUT_FORMATTER_HPP
#define OUTPUT_FORMATTER_HPP

#include "benchmark/benchmark_result.hpp"
#include <string>

namespace video_bench {

class OutputFormatter {
public:
    // Print system and video information header
    static void printHeader(const BenchmarkResult& result);

    // Print "Testing..." line
    static void printTestingStart();

    // Print a single test result line
    static void printTestResult(const StreamTestResult& result);

    // Print the final summary
    static void printSummary(const BenchmarkResult& result);

    // Print an error message
    static void printError(const std::string& message);
};

} // namespace video_bench

#endif // OUTPUT_FORMATTER_HPP
