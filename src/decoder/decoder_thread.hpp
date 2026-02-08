#ifndef DECODER_THREAD_HPP
#define DECODER_THREAD_HPP

#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <barrier>
#include <functional>

namespace video_bench {

// Thread-safe results from a decoder thread
struct DecoderThreadResult {
    int thread_id;
    int64_t frames_decoded;
    double fps;
    bool success;
    std::string error_message;
    int64_t lag_count;    // Number of frames that were late
    double max_lag_ms;    // Maximum lag in milliseconds
};

// A worker thread that continuously decodes video
class DecoderThread {
public:
    DecoderThread(int thread_id,
                  const std::string& video_path,
                  double target_fps,
                  int decoder_thread_count,
                  bool is_live_stream,
                  std::barrier<>& start_barrier,
                  std::atomic<bool>& stop_flag);

    ~DecoderThread();

    // Non-copyable, non-movable (owns thread)
    DecoderThread(const DecoderThread&) = delete;
    DecoderThread& operator=(const DecoderThread&) = delete;
    DecoderThread(DecoderThread&&) = delete;
    DecoderThread& operator=(DecoderThread&&) = delete;

    // Get accumulated frames decoded so far
    int64_t getFramesDecoded() const;

    // Get result after thread has stopped
    DecoderThreadResult getResult() const;

    // Check if thread had initialization error
    bool hasError() const;

    // Wait for thread to complete (must be called after stop_flag is set)
    void join();

private:
    void run();

    int thread_id_;
    std::string video_path_;
    double target_fps_;
    int decoder_thread_count_;
    bool is_live_stream_;
    std::barrier<>& start_barrier_;
    std::atomic<bool>& stop_flag_;

    std::atomic<int64_t> frames_decoded_{0};
    std::atomic<bool> has_error_{false};
    std::string error_message_;
    double final_fps_ = 0.0;
    int64_t lag_count_ = 0;
    double max_lag_ms_ = 0.0;

    std::jthread thread_;
};

} // namespace video_bench

#endif // DECODER_THREAD_HPP
