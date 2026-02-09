#include "decoder/packet_reader.hpp"
#include <chrono>

namespace video_bench {

PacketReader::PacketReader(const std::string& path,
                           PacketQueue& queue,
                           std::atomic<bool>& stop_flag,
                           bool is_live_stream)
    : path_(path)
    , queue_(queue)
    , stop_flag_(stop_flag)
    , is_live_stream_(is_live_stream)
    , packet_(av_packet_alloc()) {
}

bool PacketReader::init(std::string& error_message) {
    AVDictionary* options = is_live_stream_ ? createRtspOptions() : nullptr;

    // Open input
    AVFormatContext* format_ctx_raw = nullptr;
    int ret = avformat_open_input(&format_ctx_raw, path_.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0) {
        error_message = "Reader: failed to open source: " + ffmpegErrorString(ret);
        return false;
    }
    format_ctx_.reset(format_ctx_raw);

    // Find stream info
    ret = avformat_find_stream_info(format_ctx_.get(), nullptr);
    if (ret < 0) {
        error_message = "Reader: failed to find stream info: " + ffmpegErrorString(ret);
        return false;
    }

    // Find video stream
    video_stream_index_ = -1;
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = static_cast<int>(i);
            codec_params_ = format_ctx_->streams[i]->codecpar;
            break;
        }
    }

    if (video_stream_index_ < 0) {
        error_message = "Reader: no video stream found";
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
                // Signal decoder to flush stale reference frames before new loop
                queue_.pushFlushMarker(100ms);
                continue;
            } else {
                error_message_ = "Read error: " + ffmpegErrorString(ret);
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

int PacketReader::getVideoStreamIndex() const {
    return video_stream_index_;
}

const AVCodecParameters* PacketReader::getCodecParameters() const {
    return codec_params_;
}

} // namespace video_bench
