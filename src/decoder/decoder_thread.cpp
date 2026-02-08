#include "decoder/decoder_thread.hpp"
#include "decoder/video_decoder.hpp"
#include <chrono>

namespace video_bench {

DecoderThread::DecoderThread(int thread_id,
                             const std::string& video_path,
                             std::barrier<>& start_barrier,
                             std::atomic<bool>& stop_flag)
    : thread_id_(thread_id)
    , video_path_(video_path)
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
        error_message_
    };
}

bool DecoderThread::hasError() const {
    return has_error_.load(std::memory_order_relaxed);
}

void DecoderThread::run() {
    // Create decoder - each thread has its own instance
    VideoDecoder decoder;

    // Open video file
    std::string error;
    if (!decoder.open(video_path_, error)) {
        error_message_ = error;
        has_error_.store(true, std::memory_order_release);
        // Still need to arrive at barrier to not deadlock other threads
        start_barrier_.arrive_and_wait();
        return;
    }

    // Wait for all threads to be ready
    start_barrier_.arrive_and_wait();

    auto start_time = std::chrono::steady_clock::now();
    int64_t total_frames = 0;

    // Decode continuously until stop flag is set
    while (!stop_flag_.load(std::memory_order_relaxed)) {
        // Decode in small chunks to check stop flag frequently
        DecodeResult result = decoder.decodeFor(0.1);  // 100ms chunks

        if (!result.error_message.empty()) {
            error_message_ = result.error_message;
            has_error_.store(true, std::memory_order_release);
            break;
        }

        total_frames += result.frames_decoded;
        frames_decoded_.store(total_frames, std::memory_order_relaxed);
    }

    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    if (elapsed > 0) {
        final_fps_ = static_cast<double>(total_frames) / elapsed;
    }
}

} // namespace video_bench
