#include "decoder/packet_queue.hpp"

namespace video_bench {

PacketQueue::PacketQueue(size_t max_size)
    : max_size_(max_size) {
}

PacketQueue::~PacketQueue() {
    clear();
}

bool PacketQueue::push(AVPacket* packet, std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);

    // Wait for space in queue
    bool space_available = not_full_.wait_for(lock, timeout, [this] {
        return queue_.size() < max_size_ || eof_;
    });

    if (!space_available || eof_) {
        return false;
    }

    // Clone packet for queue ownership
    AVPacket* cloned = av_packet_clone(packet);
    if (!cloned) {
        return false;
    }

    queue_.push(cloned);
    not_empty_.notify_one();
    return true;
}

bool PacketQueue::pushFlushMarker(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);

    bool space_available = not_full_.wait_for(lock, timeout, [this] {
        return queue_.size() < max_size_ || eof_;
    });

    if (!space_available || eof_) {
        return false;
    }

    queue_.push(nullptr);
    not_empty_.notify_one();
    return true;
}

void PacketQueue::signalEof() {
    std::lock_guard lock(mutex_);
    eof_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

void PacketQueue::setSpaceCallback(SpaceCallback callback) {
    space_callback_ = std::move(callback);
}

std::optional<AVPacket*> PacketQueue::pop(std::chrono::milliseconds timeout) {
    AVPacket* packet = nullptr;
    bool did_pop = false;

    {
        std::unique_lock lock(mutex_);

        // Wait for packet or EOF
        bool has_data = not_empty_.wait_for(lock, timeout, [this] {
            return !queue_.empty() || eof_;
        });

        if (has_data && !queue_.empty()) {
            packet = queue_.front();
            queue_.pop();
            not_full_.notify_one();
            did_pop = true;
        }
    }

    if (did_pop) {
        // Signal reader pool after releasing queue lock to avoid deadlock
        if (space_callback_) {
            space_callback_();
        }
        return packet;
    }
    return std::nullopt;
}

bool PacketQueue::isEof() const {
    std::lock_guard lock(mutex_);
    return eof_ && queue_.empty();
}

size_t PacketQueue::size() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

void PacketQueue::clear() {
    std::lock_guard lock(mutex_);
    while (!queue_.empty()) {
        AVPacket* pkt = queue_.front();
        queue_.pop();
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
}

} // namespace video_bench
