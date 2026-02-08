#include "decoder/decoder_thread.hpp"
#include "decoder/video_decoder.hpp"
#include <chrono>

namespace video_bench {

DecoderThread::DecoderThread(int thread_id,
                             const std::string& video_path,
                             double target_fps,
                             int decoder_thread_count,
                             bool is_live_stream,
                             std::barrier<>& start_barrier,
                             std::atomic<bool>& stop_flag)
    : thread_id_(thread_id)
    , video_path_(video_path)
    , target_fps_(target_fps)
    , decoder_thread_count_(decoder_thread_count)
    , is_live_stream_(is_live_stream)
    , start_barrier_(start_barrier)
    , stop_flag_(stop_flag)
    , thread_([this] { run(); }) {
}

DecoderThread::~DecoderThread() {
    // jthread automatically joins on destruction
}

int64_t DecoderThread::getFramesDecoded() const {
    return frames_decoded_.load(std::memory_order_relaxed);
}

DecoderThreadResult DecoderThread::getResult() const {
    return {
        thread_id_,
        frames_decoded_.load(),
        final_fps_,
        !has_error_.load(),
        error_message_,
        lag_count_,
        max_lag_ms_
    };
}

bool DecoderThread::hasError() const {
    return has_error_.load(std::memory_order_relaxed);
}

void DecoderThread::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void DecoderThread::run() {
    using Clock = std::chrono::steady_clock;
    using Nanoseconds = std::chrono::nanoseconds;

    // Create decoder - each thread has its own instance
    VideoDecoder decoder;

    // Open video file
    std::string error;
    if (!decoder.open(video_path_, error, decoder_thread_count_, is_live_stream_)) {
        error_message_ = error;
        has_error_.store(true, std::memory_order_release);
        // Still need to arrive at barrier to not deadlock other threads
        start_barrier_.arrive_and_wait();
        return;
    }

    // Calculate frame interval based on target FPS (in nanoseconds for precision)
    const auto frame_interval = std::chrono::duration_cast<Nanoseconds>(
        std::chrono::duration<double>(1.0 / target_fps_));
    // Allow 1ms tolerance before counting as lag
    const auto lag_tolerance = std::chrono::milliseconds(1);

    // Wait for all threads to be ready
    start_barrier_.arrive_and_wait();

    auto start_time = Clock::now();
    auto next_frame_time = start_time;
    int64_t total_frames = 0;

    // Batch size for atomic updates and stop flag checks
    // Reduces cache line invalidation and polling overhead
    constexpr int kBatchSize = 16;

    // Decode at real-time pace until stop flag is set
    while (true) {
        // Check stop flag every kBatchSize frames to reduce cache contention
        if ((total_frames % kBatchSize) == 0) {
            if (stop_flag_.load(std::memory_order_relaxed)) {
                break;
            }
        }

        // Decode exactly one frame
        SingleFrameResult result = decoder.decodeOneFrame();

        if (!result.success) {
            error_message_ = result.error_message;
            has_error_.store(true, std::memory_order_release);
            break;
        }

        total_frames++;

        // Publish frame count every kBatchSize frames to reduce atomic overhead
        if ((total_frames % kBatchSize) == 0) {
            frames_decoded_.store(total_frames, std::memory_order_relaxed);
        }

        // Calculate when next frame should be decoded
        next_frame_time += frame_interval;

        auto now = Clock::now();

        // Check for lag (decode took longer than frame interval)
        if (now > next_frame_time + lag_tolerance) {
            lag_count_++;
            double lag_ms = std::chrono::duration<double, std::milli>(
                now - next_frame_time).count();
            if (lag_ms > max_lag_ms_) {
                max_lag_ms_ = lag_ms;
            }
            // Reset timing to prevent accumulating lag
            next_frame_time = now;
        } else if (now < next_frame_time) {
            // Ahead of schedule - sleep until next frame time
            std::this_thread::sleep_until(next_frame_time);
        }
    }

    // Final update to ensure accurate frame count after loop exits
    frames_decoded_.store(total_frames, std::memory_order_relaxed);

    auto end_time = Clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    if (elapsed > 0) {
        final_fps_ = static_cast<double>(total_frames) / elapsed;
    }
}

} // namespace video_bench
