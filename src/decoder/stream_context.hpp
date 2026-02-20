#ifndef STREAM_CONTEXT_HPP
#define STREAM_CONTEXT_HPP

#include "decoder/packet_queue.hpp"
#include "decoder/packet_reader.hpp"
#include "decoder/video_decoder.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace video_bench {

// Per-stream state for pool-based decoding.
// Each stream owns its I/O pipeline (reader thread) and decoder.
// Workers claim exclusive access via atomic CAS.
struct StreamContext {
    int stream_id = 0;

    // I/O pipeline (reader serviced by reader pool threads)
    std::unique_ptr<PacketQueue> queue;
    std::unique_ptr<PacketReader> reader;

    // Decoder (exclusive access via claimed flag)
    std::unique_ptr<VideoDecoder> decoder;

    // Real-time pacing state
    std::chrono::steady_clock::time_point next_frame_time;
    std::chrono::nanoseconds frame_interval{0};

    // Metrics
    std::atomic<int64_t> frames_decoded{0};
    int64_t total_frames = 0;
    int64_t lag_count = 0;
    double max_lag_ms = 0.0;

    // Error/completion state
    std::atomic<bool> has_error{false};
    std::string error_message;
    std::atomic<bool> finished{false};

    // Worker exclusion: CAS-based claim (no mutex needed)
    std::atomic<bool> claimed{false};

    // Try to claim exclusive access. Returns true if successfully claimed.
    bool tryClaim() {
        bool expected = false;
        return claimed.compare_exchange_strong(expected, true,
                                               std::memory_order_acquire,
                                               std::memory_order_relaxed);
    }

    // Release exclusive access.
    void release() {
        claimed.store(false, std::memory_order_release);
    }

    // Check if this stream is ready for a worker to process.
    bool isReady(std::chrono::steady_clock::time_point now) const {
        return !finished.load(std::memory_order_relaxed)
            && !has_error.load(std::memory_order_relaxed)
            && !claimed.load(std::memory_order_relaxed)
            && now >= next_frame_time;
    }
};

} // namespace video_bench

#endif // STREAM_CONTEXT_HPP
