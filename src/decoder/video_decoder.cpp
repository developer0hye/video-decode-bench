#include "decoder/video_decoder.hpp"
#include <chrono>

namespace video_bench {

VideoDecoder::VideoDecoder()
    : frame_(av_frame_alloc(), AVFrameDeleter{})
    , packet_(av_packet_alloc(), AVPacketDeleter{}) {
}

bool VideoDecoder::open(const std::string& file_path, std::string& error_message) {
    // Open input file
    AVFormatContext* format_ctx_raw = nullptr;
    int ret = avformat_open_input(&format_ctx_raw, file_path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Failed to open file: " + std::string(err_buf);
        return false;
    }
    format_ctx_.reset(format_ctx_raw);

    // Find stream info
    ret = avformat_find_stream_info(format_ctx_.get(), nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Failed to find stream info: " + std::string(err_buf);
        return false;
    }

    // Find video stream
    video_stream_index_ = -1;
    AVCodecParameters* codec_params = nullptr;

    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = static_cast<int>(i);
            codec_params = format_ctx_->streams[i]->codecpar;
            break;
        }
    }

    if (video_stream_index_ < 0) {
        error_message = "No video stream found";
        return false;
    }

    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        error_message = "Unsupported codec";
        return false;
    }

    // Allocate codec context
    AVCodecContext* codec_ctx_raw = avcodec_alloc_context3(codec);
    if (!codec_ctx_raw) {
        error_message = "Failed to allocate codec context";
        return false;
    }
    codec_ctx_.reset(codec_ctx_raw);

    // Copy codec parameters
    ret = avcodec_parameters_to_context(codec_ctx_.get(), codec_params);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Failed to copy codec params: " + std::string(err_buf);
        return false;
    }

    // Disable hardware acceleration and multi-threading
    // This ensures we measure pure CPU software decoding performance
    codec_ctx_->thread_count = 1;
    codec_ctx_->thread_type = 0;

    // Open codec
    ret = avcodec_open2(codec_ctx_.get(), codec, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Failed to open codec: " + std::string(err_buf);
        return false;
    }

    // Verify frame and packet allocation
    if (!frame_ || !packet_) {
        error_message = "Failed to allocate frame or packet";
        return false;
    }

    is_open_ = true;
    return true;
}

bool VideoDecoder::isOpen() const {
    return is_open_;
}

int64_t VideoDecoder::decodePacket(std::string* error_out) {
    int64_t frames = 0;

    int ret = avcodec_send_packet(codec_ctx_.get(), packet_.get());
    if (ret < 0) {
        // EAGAIN means decoder has buffered frames to output first - not an error
        if (ret != AVERROR(EAGAIN)) {
            if (error_out) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                *error_out = "send_packet error: " + std::string(err_buf);
            }
            return frames;
        }
    }

    while (true) {
        ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // Need more input or end of stream - normal conditions
            break;
        } else if (ret < 0) {
            // Actual decode error
            if (error_out) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                *error_out = "receive_frame error: " + std::string(err_buf);
            }
            break;
        }

        frames++;
        av_frame_unref(frame_.get());
    }

    return frames;
}

DecodeResult VideoDecoder::decodeFor(double duration_seconds) {
    DecodeResult result{0, false, ""};

    if (!is_open_) {
        result.error_message = "Decoder not open";
        return result;
    }

    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::duration<double>(duration_seconds);

    while (std::chrono::steady_clock::now() < end_time) {
        int ret = av_read_frame(format_ctx_.get(), packet_.get());

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // Drain decoder - send null packet to flush
                int drain_ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
                if (drain_ret < 0 && drain_ret != AVERROR_EOF) {
                    char err_buf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(drain_ret, err_buf, sizeof(err_buf));
                    result.error_message = "Drain send_packet error: " + std::string(err_buf);
                    return result;
                }

                // Receive all remaining frames
                while (true) {
                    int recv_ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
                    if (recv_ret == AVERROR_EOF || recv_ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (recv_ret < 0) {
                        char err_buf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(recv_ret, err_buf, sizeof(err_buf));
                        result.error_message = "Drain receive_frame error: " + std::string(err_buf);
                        return result;
                    }
                    result.frames_decoded++;
                    av_frame_unref(frame_.get());
                }

                result.reached_eof = true;

                // Seek to start for continuous decoding
                if (!seekToStart()) {
                    result.error_message = "Failed to seek to start";
                    return result;
                }
                continue;
            } else {
                char err_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err_buf, sizeof(err_buf));
                result.error_message = "Read error: " + std::string(err_buf);
                return result;
            }
        }

        // Only process video stream packets
        if (packet_->stream_index == video_stream_index_) {
            std::string decode_error;
            result.frames_decoded += decodePacket(&decode_error);
            if (!decode_error.empty()) {
                result.error_message = decode_error;
                av_packet_unref(packet_.get());
                return result;
            }
        }

        av_packet_unref(packet_.get());
    }

    return result;
}

bool VideoDecoder::seekToStart() {
    if (!is_open_) {
        return false;
    }

    // Flush codec buffers
    avcodec_flush_buffers(codec_ctx_.get());

    // Seek to beginning
    int ret = av_seek_frame(format_ctx_.get(), video_stream_index_, 0, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        // Try seeking with format context
        ret = avformat_seek_file(format_ctx_.get(), -1, INT64_MIN, 0, INT64_MAX, 0);
        if (ret < 0) {
            return false;
        }
    }

    return true;
}

} // namespace video_bench
