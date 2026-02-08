#include "decoder/packet_reader.hpp"
#include <chrono>

namespace video_bench {

namespace {
void packet_deleter(AVPacket* pkt) {
    if (pkt) {
        av_packet_free(&pkt);
    }
}
} // namespace

PacketReader::PacketReader(const std::string& path,
                           PacketQueue& queue,
                           std::atomic<bool>& stop_flag,
                           bool is_live_stream,
                           int video_stream_index)
    : path_(path)
    , queue_(queue)
    , stop_flag_(stop_flag)
    , is_live_stream_(is_live_stream)
    , video_stream_index_(video_stream_index)
    , packet_(av_packet_alloc(), packet_deleter) {
}

bool PacketReader::init(std::string& error_message) {
    // Set RTSP-specific options for live streams
    AVDictionary* options = nullptr;
    if (is_live_stream_) {
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "stimeout", "5000000", 0);
    }

    // Open input
    AVFormatContext* format_ctx_raw = nullptr;
    int ret = avformat_open_input(&format_ctx_raw, path_.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Reader: failed to open source: " + std::string(err_buf);
        return false;
    }
    format_ctx_.reset(format_ctx_raw);

    // Find stream info
    ret = avformat_find_stream_info(format_ctx_.get(), nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Reader: failed to find stream info: " + std::string(err_buf);
        return false;
    }

    if (!packet_) {
        error_message = "Reader: failed to allocate packet";
        return false;
    }

    return true;
}

void PacketReader::run() {
    using namespace std::chrono_literals;

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        int ret = av_read_frame(format_ctx_.get(), packet_.get());

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                if (is_live_stream_) {
                    // Live stream ended
                    error_message_ = "Stream ended";
                    has_error_.store(true, std::memory_order_release);
                    break;
                }
                // File mode: seek to start and continue
                avformat_seek_file(format_ctx_.get(), -1, INT64_MIN, 0, INT64_MAX, 0);
                continue;
            } else {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                error_message_ = "Read error: " + std::string(err_buf);
                has_error_.store(true, std::memory_order_release);
                break;
            }
        }

        // Only queue video packets
        if (packet_->stream_index == video_stream_index_) {
            // Push with timeout to allow checking stop flag
            if (!queue_.push(packet_.get(), 100ms)) {
                av_packet_unref(packet_.get());
                // Check if we should stop
                if (stop_flag_.load(std::memory_order_relaxed)) {
                    break;
                }
                // Queue might be full, retry
                continue;
            }
        }

        av_packet_unref(packet_.get());
    }

    // Signal EOF to decoder
    queue_.signalEof();
}

bool PacketReader::hasError() const {
    return has_error_.load(std::memory_order_acquire);
}

std::string PacketReader::getError() const {
    return error_message_;
}

} // namespace video_bench
