#ifndef DECODER_POOL_HPP
#define DECODER_POOL_HPP

#include "decoder/decoder_thread.hpp"
#include "decoder/stream_context.hpp"
#include <atomic>
#include <barrier>
#include <memory>
#include <string>
#include <vector>

namespace video_bench {

// Pool-based decoder for high stream counts.
// Uses R reader pool threads + W worker threads to service N streams,
// reducing OS thread count from 2N+1 to R+W+1.
class DecoderPool {
public:
    DecoderPool(int stream_count,
                const std::string& video_path,
                double target_fps,
                int decoder_thread_count,
                bool is_live_stream,
                std::barrier<>& start_barrier,
                std::atomic<bool>& stop_flag,
                unsigned int worker_count,
                unsigned int reader_count);

    ~DecoderPool();

    // Non-copyable, non-movable
    DecoderPool(const DecoderPool&) = delete;
    DecoderPool& operator=(const DecoderPool&) = delete;
    DecoderPool(DecoderPool&&) = delete;
    DecoderPool& operator=(DecoderPool&&) = delete;

    bool hasInitError() const;
    std::string getInitError() const;

    // Get accumulated frames decoded for a specific stream
    int64_t getStreamFrames(int stream_id) const;

    // Get results after pool has stopped (same interface as DecoderThread)
    std::vector<DecoderThreadResult> getResults() const;

    // Wait for all workers and readers to finish
    void join();

private:
    void workerLoop(int worker_id);
    void workerLoopSingle(StreamContext& ctx);
    bool drainUntilFrame(StreamContext& ctx, std::chrono::milliseconds pop_timeout);
    void readerLoop(int reader_id);

    std::vector<std::unique_ptr<StreamContext>> streams_;
    std::vector<std::thread> workers_;
    std::vector<std::thread> reader_threads_;
    std::barrier<>& start_barrier_;
    std::atomic<bool>& stop_flag_;
    unsigned int worker_count_;
    unsigned int reader_count_;
    int stream_count_;
    double target_fps_;

    std::chrono::steady_clock::time_point start_time_;
    std::atomic<bool> init_done_{false};

    // CV for reader pool: signaled when any consumer pops (creating queue space)
    std::condition_variable reader_cv_;
    std::mutex reader_cv_mutex_;

    bool init_error_ = false;
    std::string init_error_message_;
};

} // namespace video_bench

#endif // DECODER_POOL_HPP
