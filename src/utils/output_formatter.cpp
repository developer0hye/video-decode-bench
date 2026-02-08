#include "utils/output_formatter.hpp"
#include <iostream>
#include <iomanip>
#include <format>

namespace video_bench {

void OutputFormatter::printHeader(const BenchmarkResult& result) {
    std::cout << "CPU: " << result.cpu_name
              << " (" << result.thread_count << " threads)\n";
    std::cout << "Video: " << result.video_resolution
              << " " << result.codec_name
              << ", " << static_cast<int>(result.video_fps) << "fps\n";
    std::cout << "\n";
}

void OutputFormatter::printTestingStart() {
    std::cout << "Testing...\n";
}

void OutputFormatter::printTestResult(const StreamTestResult& result) {
    // Format: " N stream(s): XXXfps (CPU: YY%) [status]"
    std::string stream_word = result.stream_count == 1 ? "stream: " : "streams:";

    std::cout << std::setw(2) << result.stream_count << " " << stream_word
              << std::setw(5) << static_cast<int>(result.fps_per_stream) << "fps"
              << " (CPU: " << std::setw(2) << static_cast<int>(result.cpu_usage) << "%)"
              << " " << result.getStatusSymbol();

    if (!result.passed) {
        std::cout << " " << result.getFailureReason();
    }

    std::cout << "\n";
}

void OutputFormatter::printSummary(const BenchmarkResult& result) {
    std::cout << "\n";
    if (result.max_streams > 0) {
        std::cout << "Result: Maximum " << result.max_streams
                  << " concurrent stream" << (result.max_streams == 1 ? "" : "s")
                  << " can be decoded in real-time\n";
    } else {
        std::cout << "Result: Could not achieve real-time decoding even with 1 stream\n";
    }
}

void OutputFormatter::printError(const std::string& message) {
    std::cerr << "Error: " << message << "\n";
}

} // namespace video_bench
