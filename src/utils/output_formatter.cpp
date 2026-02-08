#include "utils/output_formatter.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace {

void printInfoLine(const std::string& line) {
    std::cout << line << "\n";
    video_bench::Logger::info(line);
}

} // namespace

namespace video_bench {

void OutputFormatter::printHeader(const BenchmarkResult& result) {
    std::ostringstream cpu_line;
    cpu_line << "CPU: " << result.cpu_name
             << " (" << result.thread_count << " threads)";
    printInfoLine(cpu_line.str());

    std::ostringstream video_line;
    video_line << (result.is_live_stream ? "Source: " : "Video: ")
               << result.video_resolution
               << " " << result.codec_name
               << ", " << static_cast<int>(result.video_fps) << "fps";
    if (result.is_live_stream) {
        video_line << " (live)";
    }
    printInfoLine(video_line.str());

    std::cout << "\n";
}

void OutputFormatter::printTestingStart() {
    printInfoLine("Testing...");
}

void OutputFormatter::printTestResult(const StreamTestResult& result) {
    // Format: " N stream(s): XXXfps (min:XX/avg:XX/max:XX) (CPU: YY%) [status]"
    std::string stream_word = result.stream_count == 1 ? "stream: " : "streams:";

    std::ostringstream line;
    line << std::setw(2) << result.stream_count << " " << stream_word
         << std::setw(5) << static_cast<int>(result.fps_per_stream) << "fps"
         << " (min:" << static_cast<int>(result.min_fps)
         << "/avg:" << static_cast<int>(result.fps_per_stream)
         << "/max:" << static_cast<int>(result.max_fps) << ")"
         << " (CPU: " << std::setw(2) << static_cast<int>(result.cpu_usage) << "%)"
         << " " << result.getStatusSymbol();

    if (!result.passed) {
        line << " " << result.getFailureReason();
    }

    printInfoLine(line.str());
}

void OutputFormatter::printSummary(const BenchmarkResult& result) {
    std::cout << "\n";

    std::ostringstream line;
    if (result.max_streams > 0) {
        line << "Result: Maximum " << result.max_streams
             << " concurrent stream" << (result.max_streams == 1 ? "" : "s")
             << " can be decoded in real-time";
    } else {
        line << "Result: Could not achieve real-time decoding even with 1 stream";
    }

    printInfoLine(line.str());
}

void OutputFormatter::printError(const std::string& message) {
    const std::string line = "Error: " + message;
    std::cerr << line << "\n";
    Logger::error(line);
}

} // namespace video_bench
