#include "decoder/decoder_pool.hpp"
#include <chrono>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace video_bench {

DecoderPool::DecoderPool(int stream_count,
                         const std::string& video_path,
                         double target_fps,
                         int decoder_thread_count,
                         bool is_live_stream,
                         std::barrier<>& start_barrier,
                         std::atomic<bool>& stop_flag,
                         unsigned int worker_count,
                         unsigned int reader_count)
    : start_barrier_(start_barrier)
    , stop_flag_(stop_flag)
    , worker_count_(worker_count)
    , reader_count_(reader_count)
    , stream_count_(stream_count)
    , target_fps_(target_fps) {
    using Nanoseconds = std::chrono::nanoseconds;

    const auto frame_interval = std::chrono::duration_cast<Nanoseconds>(
        std::chrono::duration<double>(1.0 / target_fps));

    // Initialize all stream contexts
    streams_.reserve(stream_count);
    for (int i = 0; i < stream_count; i++) {
        auto ctx = std::make_unique<StreamContext>();
        ctx->stream_id = i;
        ctx->frame_interval = frame_interval;

        // Create packet queue
        ctx->queue = std::make_unique<PacketQueue>(32);

        // Create and initialize reader
        ctx->reader = std::make_unique<PacketReader>(
            video_path, *ctx->queue, stop_flag_, is_live_stream);

        std::string error;
        if (!ctx->reader->init(error)) {
            init_error_ = true;
            init_error_message_ = "Stream " + std::to_string(i) + ": " + error;
            streams_.push_back(std::move(ctx));
            for (int j = i + 1; j < stream_count; j++) {
                streams_.push_back(std::make_unique<StreamContext>());
            }
            return;
        }

        // Create and initialize decoder
        ctx->decoder = std::make_unique<VideoDecoder>();
        if (!ctx->decoder->initFromParams(ctx->reader->getCodecParameters(), error,
                                          decoder_thread_count, is_live_stream)) {
            init_error_ = true;
            init_error_message_ = "Stream " + std::to_string(i) + ": " + error;
            streams_.push_back(std::move(ctx));
            for (int j = i + 1; j < stream_count; j++) {
                streams_.push_back(std::make_unique<StreamContext>());
            }
            return;
        }

        streams_.push_back(std::move(ctx));
    }

    // Wire up space callbacks for CV-based reader wake-up
    for (auto& ctx : streams_) {
        if (ctx->queue) {
            ctx->queue->setSpaceCallback([this] {
                reader_cv_.notify_one();
            });
        }
    }

    // Start reader threads: use 1:1 mode if reader_count >= stream_count,
    // otherwise use pooled readers
    if (reader_count_ >= static_cast<unsigned int>(stream_count)) {
        // 1:1 reader threads (same as baseline)
        reader_threads_.reserve(stream_count);
        for (int i = 0; i < stream_count; i++) {
            if (streams_[i]->reader) {
                reader_threads_.emplace_back([&reader = *streams_[i]->reader] {
                    reader.run();
                });
            }
        }
    } else {
        // Pooled reader threads (R threads service N readers)
        reader_threads_.reserve(reader_count_);
        for (unsigned int r = 0; r < reader_count_; r++) {
            reader_threads_.emplace_back([this, r] {
                readerLoop(static_cast<int>(r));
            });
        }
    }

    // Start worker threads
    workers_.reserve(worker_count_);
    for (unsigned int w = 0; w < worker_count_; w++) {
        workers_.emplace_back([this, w] { workerLoop(static_cast<int>(w)); });
    }
}

DecoderPool::~DecoderPool() {
    join();
}

bool DecoderPool::hasInitError() const {
    return init_error_;
}

std::string DecoderPool::getInitError() const {
    return init_error_message_;
}

int64_t DecoderPool::getStreamFrames(int stream_id) const {
    if (stream_id >= 0 && stream_id < stream_count_) {
        return streams_[stream_id]->frames_decoded.load(std::memory_order_relaxed);
    }
    return 0;
}

void DecoderPool::workerLoop(int worker_id) {
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    if (init_error_) {
        start_barrier_.arrive_and_wait();
        return;
    }

    start_barrier_.arrive_and_wait();

    // Only worker 0 initializes shared state (no data race)
    if (worker_id == 0) {
        start_time_ = Clock::now();
        for (auto& ctx : streams_) {
            ctx->next_frame_time = start_time_;
        }
        init_done_.store(true, std::memory_order_release);
    } else {
        while (!init_done_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    // Dedicated stream assignment: each worker owns streams in round-robin
    std::vector<int> my_streams;
    for (int i = worker_id; i < stream_count_; i += static_cast<int>(worker_count_)) {
        my_streams.push_back(i);
    }

    // Single-stream fast path: mirrors DecoderThread::run() for minimal overhead
    if (my_streams.size() == 1) {
        workerLoopSingle(*streams_[my_streams[0]]);
        return;
    }

    // Multi-stream path: scan assigned streams with pacing
    const auto lag_tolerance = 1ms;

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        auto now = Clock::now();
        auto earliest_next = Clock::time_point::max();
        bool any_active = false;
        bool any_starved = false;

        for (int idx : my_streams) {
            auto& ctx = *streams_[idx];

            if (ctx.finished.load(std::memory_order_relaxed) ||
                ctx.has_error.load(std::memory_order_relaxed)) {
                continue;
            }

            any_active = true;

            if (now >= ctx.next_frame_time) {
                bool got_frame = drainUntilFrame(ctx, 1ms);
                if (!got_frame &&
                    !ctx.finished.load(std::memory_order_relaxed) &&
                    !ctx.has_error.load(std::memory_order_relaxed)) {
                    any_starved = true;
                }
                now = Clock::now();
            }

            if (!ctx.finished.load(std::memory_order_relaxed) &&
                !ctx.has_error.load(std::memory_order_relaxed)) {
                if (ctx.next_frame_time < earliest_next) {
                    earliest_next = ctx.next_frame_time;
                }
            }
        }

        if (!any_active) break;

        now = Clock::now();
        if (any_starved) {
            std::this_thread::sleep_for(500us);
        } else if (earliest_next > now + lag_tolerance) {
            std::this_thread::sleep_until(earliest_next);
        }
    }
}

void DecoderPool::workerLoopSingle(StreamContext& ctx) {
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    constexpr int kBatchSize = 16;
    const auto lag_tolerance = 1ms;

    while (true) {
        if ((ctx.total_frames % kBatchSize) == 0) {
            if (stop_flag_.load(std::memory_order_relaxed)) {
                break;
            }
        }

        // Get packet with long timeout (efficient blocking, same as DecoderThread)
        auto packet_opt = ctx.queue->pop(100ms);

        if (!packet_opt) {
            if (ctx.queue->isEof()) {
                if (ctx.reader->hasError()) {
                    ctx.error_message = ctx.reader->getError();
                    ctx.has_error.store(true, std::memory_order_release);
                }
                ctx.finished.store(true, std::memory_order_release);
                break;
            }
            continue;
        }

        AVPacket* packet = *packet_opt;

        if (!packet) {
            ctx.decoder->flushBuffers();
            continue;
        }

        SingleFrameResult result = ctx.decoder->decodeFromPacket(packet);
        av_packet_free(&packet);

        if (!result.error_message.empty()) {
            ctx.error_message = result.error_message;
            ctx.has_error.store(true, std::memory_order_release);
            break;
        }

        if (!result.success) {
            continue;
        }

        ctx.total_frames++;

        if ((ctx.total_frames % kBatchSize) == 0) {
            ctx.frames_decoded.store(ctx.total_frames, std::memory_order_relaxed);
        }

        // Pacing: advance and sleep (same as DecoderThread)
        ctx.next_frame_time += ctx.frame_interval;
        auto now = Clock::now();

        if (now > ctx.next_frame_time + lag_tolerance) {
            ctx.lag_count++;
            double lag_ms = std::chrono::duration<double, std::milli>(
                now - ctx.next_frame_time).count();
            if (lag_ms > ctx.max_lag_ms) {
                ctx.max_lag_ms = lag_ms;
            }
            ctx.next_frame_time = now;
        } else if (now < ctx.next_frame_time) {
            std::this_thread::sleep_until(ctx.next_frame_time);
        }
    }
}

bool DecoderPool::drainUntilFrame(StreamContext& ctx,
                                   std::chrono::milliseconds pop_timeout) {
    using Clock = std::chrono::steady_clock;

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        auto packet_opt = ctx.queue->pop(pop_timeout);

        if (!packet_opt) {
            if (ctx.queue->isEof()) {
                if (ctx.reader->hasError()) {
                    ctx.error_message = ctx.reader->getError();
                    ctx.has_error.store(true, std::memory_order_release);
                }
                ctx.finished.store(true, std::memory_order_release);
            }
            return false;
        }

        AVPacket* packet = *packet_opt;

        // Flush marker (nullptr sentinel from reader on file loop)
        if (!packet) {
            ctx.decoder->flushBuffers();
            continue;
        }

        // Decode from packet
        SingleFrameResult result = ctx.decoder->decodeFromPacket(packet);
        av_packet_free(&packet);

        if (!result.error_message.empty()) {
            ctx.error_message = result.error_message;
            ctx.has_error.store(true, std::memory_order_release);
            return false;
        }

        if (!result.success) {
            // Need more packets to produce a frame — keep draining
            continue;
        }

        // Frame decoded successfully
        ctx.total_frames++;

        constexpr int kBatchSize = 16;
        if ((ctx.total_frames % kBatchSize) == 0) {
            ctx.frames_decoded.store(ctx.total_frames, std::memory_order_relaxed);
        }

        // Advance pacing
        ctx.next_frame_time += ctx.frame_interval;
        auto now = Clock::now();

        const auto lag_tolerance = std::chrono::milliseconds(1);
        if (now > ctx.next_frame_time + lag_tolerance) {
            ctx.lag_count++;
            double lag_ms = std::chrono::duration<double, std::milli>(
                now - ctx.next_frame_time).count();
            if (lag_ms > ctx.max_lag_ms) {
                ctx.max_lag_ms = lag_ms;
            }
            ctx.next_frame_time = now;
        }

        return true;
    }
    return false;
}

void DecoderPool::readerLoop(int reader_id) {
    using namespace std::chrono_literals;

    // Assign streams in round-robin
    std::vector<int> my_readers;
    for (int i = reader_id; i < stream_count_;
         i += static_cast<int>(reader_count_)) {
        my_readers.push_back(i);
    }

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        bool any_active = false;
        bool any_did_work = false;

        for (int idx : my_readers) {
            auto& ctx = *streams_[idx];
            if (!ctx.reader) continue;

            auto result = ctx.reader->readNextPacket();
            switch (result) {
                case ReadResult::kPacketQueued:
                    any_active = true;
                    any_did_work = true;
                    break;
                case ReadResult::kQueueFull:
                    any_active = true;
                    break;
                case ReadResult::kSkipped:
                    any_active = true;
                    any_did_work = true;
                    break;
                case ReadResult::kDone:
                    break;
            }
        }

        if (!any_active) break;

        if (!any_did_work) {
            // CV-based wait: sleep with zero CPU until consumer pops
            std::unique_lock lock(reader_cv_mutex_);
            reader_cv_.wait_for(lock, 10ms, [this] {
                return stop_flag_.load(std::memory_order_relaxed);
            });
        }
    }

    // Signal EOF for all assigned readers on stop
    for (int idx : my_readers) {
        auto& ctx = *streams_[idx];
        if (ctx.reader) {
            ctx.reader->signalDone();
        }
    }
}

void DecoderPool::join() {
    // Join worker threads
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();

    // Flush decoders and finalize frame counts
    for (auto& ctx : streams_) {
        if (ctx->decoder && !ctx->has_error.load()) {
            while (true) {
                SingleFrameResult result = ctx->decoder->flushDecoder();
                if (!result.success) {
                    break;
                }
                ctx->total_frames++;
            }
        }
        ctx->frames_decoded.store(ctx->total_frames, std::memory_order_relaxed);
    }

    // Wake up reader pool threads for clean shutdown
    reader_cv_.notify_all();

    // Join reader pool threads
    for (auto& thread : reader_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    reader_threads_.clear();
}

std::vector<DecoderThreadResult> DecoderPool::getResults() const {
    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time_).count();

    std::vector<DecoderThreadResult> results;
    results.reserve(stream_count_);

    for (const auto& ctx : streams_) {
        int64_t frames = ctx->frames_decoded.load(std::memory_order_relaxed);
        double fps = (elapsed > 0) ? static_cast<double>(frames) / elapsed : 0.0;

        results.push_back({
            ctx->stream_id,
            frames,
            fps,
            !ctx->has_error.load(),
            ctx->error_message,
            ctx->lag_count,
            ctx->max_lag_ms
        });
    }

    return results;
}

} // namespace video_bench
