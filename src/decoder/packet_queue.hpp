#ifndef PACKET_QUEUE_HPP
#define PACKET_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace video_bench {

// Thread-safe bounded queue for AVPackets
// Used to decouple I/O (reading) from CPU-intensive decoding
class PacketQueue {
public:
    explicit PacketQueue(size_t max_size = 32);
    ~PacketQueue();

    // Non-copyable, non-movable
    PacketQueue(const PacketQueue&) = delete;
    PacketQueue& operator=(const PacketQueue&) = delete;
    PacketQueue(PacketQueue&&) = delete;
    PacketQueue& operator=(PacketQueue&&) = delete;

    // Producer (Reader thread) - pushes cloned packet
    // Returns false if timeout elapsed before space became available
    bool push(AVPacket* packet, std::chrono::milliseconds timeout);

    // Push a flush marker (nullptr sentinel) to signal decoder to flush buffers
    // Used on file loop boundary to reset decoder state
    bool pushFlushMarker(std::chrono::milliseconds timeout);

    // Signal EOF to consumers
    void signalEof();

    // Consumer (Decoder thread) - pops packet (caller owns returned packet)
    // Returns nullopt if timeout elapsed or EOF reached with empty queue
    std::optional<AVPacket*> pop(std::chrono::milliseconds timeout);

    // Set callback invoked after pop() creates space (for reader pool wake-up).
    // Called outside the queue's lock to avoid deadlock.
    using SpaceCallback = std::function<void()>;
    void setSpaceCallback(SpaceCallback callback);

    // Check if EOF has been signaled and queue is empty
    bool isEof() const;

    // Get current queue size
    size_t size() const;

    // Clear queue and release all packets
    void clear();

private:
    std::queue<AVPacket*> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    size_t max_size_;
    bool eof_ = false;
    SpaceCallback space_callback_;
};

} // namespace video_bench

#endif // PACKET_QUEUE_HPP
