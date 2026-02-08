#ifndef BENCHMARK_CONFIG_HPP
#define BENCHMARK_CONFIG_HPP

#include <string>
#include <optional>

namespace video_bench {

struct BenchmarkConfig {
    // Required: path to video file
    std::string video_path;

    // Optional: maximum number of streams to test (default: CPU thread count)
    std::optional<int> max_streams;

    // Optional: target FPS (default: video's native FPS)
    std::optional<double> target_fps;

    // Optional: log file path (default: video-benchmark.log)
    std::optional<std::string> log_file;

    // Measurement duration per test in seconds
    double measurement_duration = 10.0;

    // CPU usage threshold percentage
    double cpu_threshold = 85.0;
};

} // namespace video_bench

#endif // BENCHMARK_CONFIG_HPP
