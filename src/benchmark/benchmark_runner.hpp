#ifndef BENCHMARK_RUNNER_HPP
#define BENCHMARK_RUNNER_HPP

#include "benchmark/benchmark_config.hpp"
#include "benchmark/benchmark_result.hpp"
#include "video/video_info.hpp"
#include <functional>

namespace video_bench {

// Callback for progress updates
using ProgressCallback = std::function<void(const StreamTestResult&)>;

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(const BenchmarkConfig& config);

    // Run the benchmark
    // Returns the complete benchmark result
    BenchmarkResult run(ProgressCallback progress_callback = nullptr);

private:
    // Get stream counts to test (1, 2, 4, 8, 12, 16, 20, 24, ...)
    std::vector<int> getStreamCountsToTest(int max_streams) const;

    // Result of a single stream count test (internal use)
    struct SingleTestResult {
        StreamTestResult result;
        bool has_error;
        std::string error_message;
    };

    // Run a single stream count test
    SingleTestResult runSingleTest(int stream_count, double target_fps);

    BenchmarkConfig config_;
    VideoInfo video_info_;
};

} // namespace video_bench

#endif // BENCHMARK_RUNNER_HPP
