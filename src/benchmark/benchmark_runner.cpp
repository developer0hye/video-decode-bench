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

namespace {
// Allow 2% tolerance for timing overhead in real-time paced decoding
constexpr double kFpsTolerance = 0.98;
// Use single-threaded FFmpeg decoding when stream count >= this threshold
constexpr int kMultiThreadStreamThreshold = 4;
// Powers-of-2 stop at this stream count, then switch to linear steps
constexpr int kPowerOfTwoMaxStreams = 16;
// Extra step inserted between 8 and 16
constexpr int kExtraStepStreams = 12;
// Linear step size after powers of 2
constexpr int kLinearStepSize = 4;
// First linear step value
constexpr int kLinearStepStart = 20;
} // namespace

BenchmarkRunner::BenchmarkRunner(const BenchmarkConfig& config, const VideoInfo& video_info)
    : config_(config), video_info_(video_info) {
}

std::vector<int> BenchmarkRunner::getStreamCountsToTest(int max_streams) const {
    std::vector<int> counts;

    // Start with powers of 2 up to kPowerOfTwoMaxStreams
    for (int n = 1; n <= kPowerOfTwoMaxStreams && n <= max_streams; n *= 2) {
        counts.push_back(n);
    }

    // Then add kExtraStepStreams if not already included and within range
    if (max_streams >= kExtraStepStreams &&
        std::find(counts.begin(), counts.end(), kExtraStepStreams) == counts.end()) {
        counts.push_back(kExtraStepStreams);
    }

    // Then add increments of kLinearStepSize starting from kLinearStepStart
    for (int n = kLinearStepStart; n <= max_streams; n += kLinearStepSize) {
        counts.push_back(n);
    }

    // Always include max_streams to ensure we test the upper bound
    if (std::find(counts.begin(), counts.end(), max_streams) == counts.end()) {
        counts.push_back(max_streams);
    }

    // Sort the list
    std::sort(counts.begin(), counts.end());

    return counts;
}

BenchmarkRunner::SingleTestResult BenchmarkRunner::runSingleTest(int stream_count, double target_fps) {
    SingleTestResult single_result;
    single_result.has_error = false;

    // Create synchronization primitives
    std::barrier start_barrier(stream_count + 1);
    std::atomic<bool> stop_flag{false};

    // Create CPU monitor
    auto cpu_monitor = CpuMonitor::create();

    // Calculate decoder thread count based on CPU cores and stream count
    // For high stream counts (>=4), use single-threaded decoding to avoid
    // thread oversubscription (N OS threads + N*M FFmpeg threads competing)
    unsigned int cpu_cores = std::thread::hardware_concurrency();
    if (cpu_cores == 0) cpu_cores = 4;  // fallback

    int decoder_threads;
    if (stream_count >= kMultiThreadStreamThreshold) {
        // High stream count: single-threaded FFmpeg to prevent oversubscription
        decoder_threads = 1;
    } else {
        // Low stream count: allow FFmpeg multi-threading
        decoder_threads = std::max(1, static_cast<int>(cpu_cores) / stream_count);
    }

    // Create decoder threads
    std::vector<std::unique_ptr<DecoderThread>> threads;
    threads.reserve(stream_count);

    bool is_live = video_info_.is_live_stream;

    for (int i = 0; i < stream_count; i++) {
        threads.push_back(std::make_unique<DecoderThread>(
            i, config_.video_path, target_fps, decoder_threads, is_live,
            start_barrier, stop_flag));
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

    // Get CPU usage before threads finish
    double cpu_usage = cpu_monitor->getCpuUsage();

    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    // Wait for all threads to fully stop before collecting results
    // This ensures final frame counts are accurate
    for (const auto& thread : threads) {
        thread->join();
    }

    // Collect frame counts after threads have joined
    int64_t total_frames = 0;
    std::vector<int64_t> per_stream_frames;
    per_stream_frames.reserve(stream_count);

    for (const auto& thread : threads) {
        auto thread_result = thread->getResult();
        if (thread->hasError()) {
            single_result.has_error = true;
            if (single_result.error_message.empty()) {
                single_result.error_message = "Thread " + std::to_string(thread_result.thread_id)
                                            + ": " + thread_result.error_message;
            }
        }
        total_frames += thread_result.frames_decoded;
        per_stream_frames.push_back(thread_result.frames_decoded);
    }

    // Clear threads (already joined)
    threads.clear();

    calculateTestResult(single_result, per_stream_frames, total_frames,
                        elapsed, cpu_usage, stream_count, target_fps);

    return single_result;
}

void BenchmarkRunner::calculateTestResult(SingleTestResult& single_result,
                                           const std::vector<int64_t>& per_stream_frames,
                                           int64_t total_frames, double elapsed,
                                           double cpu_usage, int stream_count,
                                           double target_fps) {
    // Calculate per-stream FPS from frame counts and elapsed time
    std::vector<double> per_stream_fps;
    per_stream_fps.reserve(stream_count);
    for (int64_t frames : per_stream_frames) {
        double fps = (elapsed > 0) ? static_cast<double>(frames) / elapsed : 0.0;
        per_stream_fps.push_back(fps);
    }

    StreamTestResult& test_result = single_result.result;
    test_result.stream_count = stream_count;
    test_result.cpu_usage = cpu_usage;
    test_result.per_stream_fps = std::move(per_stream_fps);

    // Calculate min/max FPS from per-stream data
    test_result.min_fps = *std::min_element(test_result.per_stream_fps.begin(),
                                             test_result.per_stream_fps.end());
    test_result.max_fps = *std::max_element(test_result.per_stream_fps.begin(),
                                             test_result.per_stream_fps.end());

    if (elapsed > 0 && stream_count > 0) {
        double total_fps = static_cast<double>(total_frames) / elapsed;
        test_result.fps_per_stream = total_fps / stream_count;
    } else {
        test_result.fps_per_stream = 0.0;
    }

    // Check pass/fail criteria
    test_result.fps_passed = test_result.min_fps >= (target_fps * kFpsTolerance);
    test_result.cpu_passed = test_result.cpu_usage <= config_.cpu_threshold;
    test_result.passed = test_result.fps_passed && test_result.cpu_passed;
}

BenchmarkResult BenchmarkRunner::run(ProgressCallback progress_callback) {
    BenchmarkResult result;
    result.success = false;
    result.max_streams = 0;

    // Get system info
    result.cpu_name = SystemInfo::getCpuName();
    result.thread_count = SystemInfo::getThreadCount();

    // Set video info in result
    result.video_resolution = video_info_.getResolutionString();
    result.codec_name = video_info_.codec_name;
    result.video_fps = video_info_.fps;
    result.is_live_stream = video_info_.is_live_stream;

    // Determine target FPS
    result.target_fps = config_.target_fps.value_or(video_info_.fps);

    // Determine max streams to test
    int max_streams = config_.max_streams.value_or(
        static_cast<int>(result.thread_count));

    // Get stream counts to test
    auto stream_counts = getStreamCountsToTest(max_streams);

    int last_passing = 0;

    for (int count : stream_counts) {
        auto single_result = runSingleTest(count, result.target_fps);

        if (single_result.has_error) {
            result.error_message = single_result.error_message;
            result.success = false;
            return result;
        }

        result.test_results.push_back(single_result.result);

        if (progress_callback) {
            progress_callback(single_result.result);
        }

        if (single_result.result.passed) {
            last_passing = count;
        } else {
            // First failure - binary search between last_passing and count
            if (last_passing > 0 && count - last_passing > 1) {
                int low = last_passing + 1;
                int high = count - 1;

                while (low <= high) {
                    int mid = low + (high - low) / 2;
                    auto mid_result = runSingleTest(mid, result.target_fps);

                    if (mid_result.has_error) {
                        result.error_message = mid_result.error_message;
                        result.success = false;
                        return result;
                    }

                    result.test_results.push_back(mid_result.result);

                    if (progress_callback) {
                        progress_callback(mid_result.result);
                    }

                    if (mid_result.result.passed) {
                        last_passing = mid;
                        low = mid + 1;
                    } else {
                        high = mid - 1;
                    }
                }
            }
            break;
        }
    }

    result.max_streams = last_passing;
    result.success = true;

    return result;
}

} // namespace video_bench
