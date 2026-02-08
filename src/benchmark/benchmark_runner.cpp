#include "benchmark/benchmark_runner.hpp"
#include "decoder/decoder_thread.hpp"
#include "monitor/cpu_monitor.hpp"
#include "monitor/system_info.hpp"
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <barrier>
#include <atomic>
#include <algorithm>

namespace video_bench {

BenchmarkRunner::BenchmarkRunner(const BenchmarkConfig& config)
    : config_(config) {
}

std::vector<int> BenchmarkRunner::getStreamCountsToTest(int max_streams) const {
    std::vector<int> counts;

    // Start with powers of 2 up to 16
    for (int n = 1; n <= 16 && n <= max_streams; n *= 2) {
        counts.push_back(n);
    }

    // Then add 12 if not already included and within range
    if (max_streams >= 12 && std::find(counts.begin(), counts.end(), 12) == counts.end()) {
        counts.push_back(12);
    }

    // Then add increments of 4 starting from 20
    for (int n = 20; n <= max_streams; n += 4) {
        counts.push_back(n);
    }

    // Sort the list
    std::sort(counts.begin(), counts.end());

    return counts;
}

BenchmarkResult BenchmarkRunner::run(ProgressCallback progress_callback) {
    BenchmarkResult result;
    result.success = false;
    result.max_streams = 0;

    // Get system info
    result.cpu_name = SystemInfo::getCpuName();
    result.thread_count = SystemInfo::getThreadCount();

    // Analyze video
    std::string error;
    auto video_info_opt = VideoAnalyzer::analyze(config_.video_path, error);
    if (!video_info_opt) {
        result.error_message = error;
        return result;
    }
    video_info_ = *video_info_opt;

    // Check codec support
    if (!video_info_.isCodecSupported()) {
        result.error_message = "Unsupported codec: " + video_info_.codec_name;
        return result;
    }

    // Set video info in result
    result.video_resolution = video_info_.getResolutionString();
    result.codec_name = video_info_.codec_name;
    result.video_fps = video_info_.fps;

    // Determine target FPS
    result.target_fps = config_.target_fps.value_or(video_info_.fps);

    // Determine max streams to test
    int max_streams = config_.max_streams.value_or(
        static_cast<int>(result.thread_count));

    // Get stream counts to test
    auto stream_counts = getStreamCountsToTest(max_streams);

    // Create synchronization primitives and shared state that persist across iterations
    // Note: We need to properly implement the test loop

    int last_passing = 0;

    for (int count : stream_counts) {
        // Create synchronization primitives
        std::barrier start_barrier(count + 1);
        std::atomic<bool> stop_flag{false};

        // Create CPU monitor
        auto cpu_monitor = CpuMonitor::create();

        // Create decoder threads
        std::vector<std::unique_ptr<DecoderThread>> threads;
        threads.reserve(count);

        for (int i = 0; i < count; i++) {
            threads.push_back(std::make_unique<DecoderThread>(
                i, config_.video_path, start_barrier, stop_flag));
        }

        // Wait for all threads to complete setup and be ready
        start_barrier.arrive_and_wait();

        // Start CPU monitoring after threads begin decoding
        cpu_monitor->startMeasurement();
        auto start_time = std::chrono::steady_clock::now();

        // Wait for measurement duration
        std::this_thread::sleep_for(
            std::chrono::duration<double>(config_.measurement_duration));

        // Signal threads to stop
        stop_flag.store(true, std::memory_order_release);

        // Get CPU usage before threads are destroyed
        double cpu_usage = cpu_monitor->getCpuUsage();

        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();

        // Collect frame counts and per-thread FPS while threads still exist
        int64_t total_frames = 0;
        bool any_errors = false;
        std::vector<double> per_stream_fps;
        per_stream_fps.reserve(count);

        std::string first_error_message;
        for (const auto& thread : threads) {
            auto thread_result = thread->getResult();
            if (thread->hasError()) {
                any_errors = true;
                if (first_error_message.empty()) {
                    first_error_message = "Thread " + std::to_string(thread_result.thread_id)
                                        + ": " + thread_result.error_message;
                }
            }
            total_frames += thread_result.frames_decoded;
            per_stream_fps.push_back(thread_result.fps);
        }

        // Now threads can be destroyed (will join)
        threads.clear();

        if (any_errors) {
            // Decode error occurred - report and stop benchmark
            result.error_message = first_error_message;
            result.success = false;
            return result;
        }

        // Calculate results
        StreamTestResult test_result;
        test_result.stream_count = count;
        test_result.cpu_usage = cpu_usage;
        test_result.per_stream_fps = std::move(per_stream_fps);

        // Calculate min/max FPS from per-stream data
        test_result.min_fps = *std::min_element(test_result.per_stream_fps.begin(),
                                                 test_result.per_stream_fps.end());
        test_result.max_fps = *std::max_element(test_result.per_stream_fps.begin(),
                                                 test_result.per_stream_fps.end());

        if (elapsed > 0 && count > 0) {
            double total_fps = static_cast<double>(total_frames) / elapsed;
            test_result.fps_per_stream = total_fps / count;  // Average FPS (for display)
        } else {
            test_result.fps_per_stream = 0.0;
        }

        // Key change: Check that minimum FPS meets target (per spec requirement)
        test_result.fps_passed = test_result.min_fps >= result.target_fps;
        test_result.cpu_passed = test_result.cpu_usage <= config_.cpu_threshold;
        test_result.passed = test_result.fps_passed && test_result.cpu_passed;

        result.test_results.push_back(test_result);

        // Report progress
        if (progress_callback) {
            progress_callback(test_result);
        }

        if (test_result.passed) {
            last_passing = count;
        } else {
            // First failure - we could do binary search here for exact limit
            // For now, just stop testing
            break;
        }
    }

    result.max_streams = last_passing;
    result.success = true;

    return result;
}

} // namespace video_bench
