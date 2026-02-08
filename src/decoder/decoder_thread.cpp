#include "decoder/decoder_thread.hpp"
#include "decoder/video_decoder.hpp"
#include "decoder/packet_queue.hpp"
#include "decoder/packet_reader.hpp"
#include <chrono>
#include <thread>

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
    using namespace std::chrono_literals;

    // Create decoder - each thread has its own instance
    VideoDecoder decoder;

    // Open video file for decoder (to get codec context)
    std::string error;
    if (!decoder.open(video_path_, error, decoder_thread_count_, is_live_stream_)) {
        error_message_ = error;
        has_error_.store(true, std::memory_order_release);
        start_barrier_.arrive_and_wait();
        return;
    }

    // Create packet queue for pipeline
    PacketQueue queue(32);

    // Create packet reader
    PacketReader reader(video_path_, queue, stop_flag_, is_live_stream_,
                        decoder.getVideoStreamIndex());

    // Initialize reader (opens file/stream)
    if (!reader.init(error)) {
        error_message_ = error;
        has_error_.store(true, std::memory_order_release);
        start_barrier_.arrive_and_wait();
        return;
    }

    // Start reader thread
    std::jthread reader_thread([&reader] { reader.run(); });

    // Calculate frame interval
    const auto frame_interval = std::chrono::duration_cast<Nanoseconds>(
        std::chrono::duration<double>(1.0 / target_fps_));
    const auto lag_tolerance = std::chrono::milliseconds(1);

    // Wait for all threads to be ready
    start_barrier_.arrive_and_wait();

    auto start_time = Clock::now();
    auto next_frame_time = start_time;
    int64_t total_frames = 0;

    constexpr int kBatchSize = 16;

    // Decode at real-time pace until stop flag is set
    while (true) {
        if ((total_frames % kBatchSize) == 0) {
            if (stop_flag_.load(std::memory_order_relaxed)) {
                break;
            }
        }

        // Get packet from queue
        auto packet_opt = queue.pop(100ms);

        if (!packet_opt) {
            // Check for EOF or reader error
            if (queue.isEof()) {
                if (reader.hasError()) {
                    error_message_ = reader.getError();
                    has_error_.store(true, std::memory_order_release);
                }
                break;
            }
            // Timeout - check stop flag and continue
            continue;
        }

        AVPacket* packet = *packet_opt;

        // Decode from packet (may produce 0 or 1 frame due to B-frames)
        SingleFrameResult result = decoder.decodeFromPacket(packet);
        av_packet_free(&packet);

        if (!result.error_message.empty()) {
            error_message_ = result.error_message;
            has_error_.store(true, std::memory_order_release);
            break;
        }

        if (!result.success) {
            // No frame yet (need more packets) - continue without timing
            continue;
        }

        total_frames++;

        if ((total_frames % kBatchSize) == 0) {
            frames_decoded_.store(total_frames, std::memory_order_relaxed);
        }

        // Timing/pacing
        next_frame_time += frame_interval;
        auto now = Clock::now();

        if (now > next_frame_time + lag_tolerance) {
            lag_count_++;
            double lag_ms = std::chrono::duration<double, std::milli>(
                now - next_frame_time).count();
            if (lag_ms > max_lag_ms_) {
                max_lag_ms_ = lag_ms;
            }
            next_frame_time = now;
        } else if (now < next_frame_time) {
            std::this_thread::sleep_until(next_frame_time);
        }
    }

    // Flush decoder to get remaining buffered frames
    while (true) {
        SingleFrameResult result = decoder.flushDecoder();
        if (!result.success) {
            break;
        }
        total_frames++;
    }

    frames_decoded_.store(total_frames, std::memory_order_relaxed);

    auto end_time = Clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    if (elapsed > 0) {
        final_fps_ = static_cast<double>(total_frames) / elapsed;
    }
}

} // namespace video_bench
