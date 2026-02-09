#ifndef BENCHMARK_RESULT_HPP
#define BENCHMARK_RESULT_HPP

#include <string>
#include <vector>

namespace video_bench {

// Result of a single stream count test
struct StreamTestResult {
    int stream_count;
    double fps_per_stream;      // Average FPS across all streams
    double min_fps;             // Minimum FPS among all streams
    double max_fps;             // Maximum FPS among all streams
    std::vector<double> per_stream_fps;  // FPS for each individual stream
    std::vector<int64_t> per_stream_frames;  // Frame count for each stream
    double cpu_usage;           // Average CPU usage percentage
    bool fps_passed;            // Met FPS requirement (based on min_fps)
    bool cpu_passed;            // Met CPU threshold
    bool passed;                // Both requirements met

    std::string getStatusSymbol() const {
        return passed ? "\xE2\x9C\x93" : "\xE2\x9C\x97";  // UTF-8 for ✓ and ✗
    }

    std::string getFailureReason() const {
        if (passed) return "";
        if (!fps_passed) return "FPS below target";
        if (!cpu_passed) return "CPU threshold exceeded";
        return "Unknown";
    }
};

// Overall benchmark result
struct BenchmarkResult {
    // System info
    std::string cpu_name;
    unsigned int thread_count;

    // Video info
    std::string video_path;
    std::string video_resolution;
    std::string codec_name;
    double video_fps;
    bool is_live_stream;

    // Target FPS used for testing
    double target_fps;

    // Results for each stream count tested
    std::vector<StreamTestResult> test_results;

    // Maximum successful stream count
    int max_streams;

    // Whether benchmark completed successfully
    bool success;
    std::string error_message;
};

} // namespace video_bench

#endif // BENCHMARK_RESULT_HPP
