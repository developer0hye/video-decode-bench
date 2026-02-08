#include "decoder/video_decoder.hpp"
#include <chrono>

namespace video_bench {

VideoDecoder::VideoDecoder()
    : frame_(av_frame_alloc(), AVFrameDeleter{})
    , packet_(av_packet_alloc(), AVPacketDeleter{}) {
}

bool VideoDecoder::open(const std::string& file_path, std::string& error_message,
                        int thread_count, bool is_live_stream) {
    is_live_stream_ = is_live_stream;

    AVDictionary* options = is_live_stream ? createRtspOptions() : nullptr;

    // Open input
    AVFormatContext* format_ctx_raw = nullptr;
    int ret = avformat_open_input(&format_ctx_raw, file_path.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0) {
        error_message = "Failed to open source: " + ffmpegErrorString(ret);
        return false;
    }
    format_ctx_.reset(format_ctx_raw);

    // Find stream info
    ret = avformat_find_stream_info(format_ctx_.get(), nullptr);
    if (ret < 0) {
        error_message = "Failed to find stream info: " + ffmpegErrorString(ret);
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
        error_message = "Failed to copy codec params: " + ffmpegErrorString(ret);
        return false;
    }

    // Configure decoder threading based on expected concurrent streams
    // thread_count=1 for many streams, higher for fewer streams
    codec_ctx_->thread_count = thread_count;
    codec_ctx_->thread_type = (thread_count == 1) ? 0 : FF_THREAD_FRAME;

    // Open codec
    ret = avcodec_open2(codec_ctx_.get(), codec, nullptr);
    if (ret < 0) {
        error_message = "Failed to open codec: " + ffmpegErrorString(ret);
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

bool VideoDecoder::initFromParams(const AVCodecParameters* codec_params,
                                   std::string& error_message,
                                   int thread_count, bool is_live_stream) {
    is_live_stream_ = is_live_stream;

    if (!codec_params) {
        error_message = "Null codec parameters";
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
    int ret = avcodec_parameters_to_context(codec_ctx_.get(), codec_params);
    if (ret < 0) {
        error_message = "Failed to copy codec params: " + ffmpegErrorString(ret);
        return false;
    }

    // Configure decoder threading
    codec_ctx_->thread_count = thread_count;
    codec_ctx_->thread_type = (thread_count == 1) ? 0 : FF_THREAD_FRAME;

    // Open codec
    ret = avcodec_open2(codec_ctx_.get(), codec, nullptr);
    if (ret < 0) {
        error_message = "Failed to open codec: " + ffmpegErrorString(ret);
        return false;
    }

    // Verify frame allocation
    if (!frame_) {
        error_message = "Failed to allocate frame";
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
                *error_out = "send_packet error: " + ffmpegErrorString(ret);
            }
            return frames;
        }
    }

    while (true) {
        ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            if (error_out) {
                *error_out = "receive_frame error: " + ffmpegErrorString(ret);
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
                    result.error_message = "Drain send_packet error: " + ffmpegErrorString(drain_ret);
                    return result;
                }

                // Receive all remaining frames
                while (true) {
                    int recv_ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
                    if (recv_ret == AVERROR_EOF || recv_ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (recv_ret < 0) {
                        result.error_message = "Drain receive_frame error: " + ffmpegErrorString(recv_ret);
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
                result.error_message = "Read error: " + ffmpegErrorString(ret);
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

SingleFrameResult VideoDecoder::handleEof() {
    SingleFrameResult result{false, false, ""};

    // Drain decoder
    int drain_ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
    if (drain_ret < 0 && drain_ret != AVERROR_EOF) {
        result.error_message = "Drain error: " + ffmpegErrorString(drain_ret);
        return result;
    }

    // Try to get remaining frames
    int ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
    if (ret == 0) {
        av_frame_unref(frame_.get());
        result.success = true;
        result.reached_eof = true;
        return result;
    }

    // Live streams cannot seek - report EOF
    if (is_live_stream_) {
        result.error_message = "Stream ended";
        return result;
    }

    // File: seek to start and continue
    if (!seekToStart()) {
        result.error_message = "Failed to seek to start";
        return result;
    }
    result.reached_eof = true;
    return result;
}

SingleFrameResult VideoDecoder::decodeOneFrame() {
    SingleFrameResult result{false, false, ""};

    if (!is_open_) {
        result.error_message = "Decoder not open";
        return result;
    }

    // Loop until we get exactly one frame
    while (true) {
        // First, try to receive a frame from decoder (might have buffered frames)
        int ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());

        if (ret == 0) {
            // Got a frame
            av_frame_unref(frame_.get());
            result.success = true;
            return result;
        } else if (ret == AVERROR(EAGAIN)) {
            // Need more input data - read next packet
            ret = av_read_frame(format_ctx_.get(), packet_.get());

            if (ret < 0) {
                if (ret == AVERROR_EOF) {
                    auto eof_result = handleEof();
                    if (!eof_result.error_message.empty() || eof_result.success) {
                        return eof_result;
                    }
                    // handleEof seeked to start, continue decoding
                    result.reached_eof = true;
                    continue;
                } else {
                    result.error_message = "Read error: " + ffmpegErrorString(ret);
                    return result;
                }
            }

            // Skip non-video packets
            if (packet_->stream_index != video_stream_index_) {
                av_packet_unref(packet_.get());
                continue;
            }

            // Send packet to decoder
            ret = avcodec_send_packet(codec_ctx_.get(), packet_.get());
            av_packet_unref(packet_.get());

            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                result.error_message = "Send packet error: " + ffmpegErrorString(ret);
                return result;
            }
            // Loop back to receive the frame
        } else if (ret == AVERROR_EOF) {
            // Decoder fully drained
            if (is_live_stream_) {
                result.error_message = "Stream ended";
                return result;
            }
            if (!seekToStart()) {
                result.error_message = "Failed to seek to start";
                return result;
            }
            result.reached_eof = true;
            continue;
        } else {
            result.error_message = "Decode error: " + ffmpegErrorString(ret);
            return result;
        }
    }
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

SingleFrameResult VideoDecoder::decodeFromPacket(AVPacket* packet) {
    SingleFrameResult result{false, false, ""};

    if (!is_open_) {
        result.error_message = "Decoder not open";
        return result;
    }

    // Send packet to decoder
    int ret = avcodec_send_packet(codec_ctx_.get(), packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        result.error_message = "Send packet error: " + ffmpegErrorString(ret);
        return result;
    }

    // Try to receive a frame
    ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
    if (ret == 0) {
        av_frame_unref(frame_.get());
        result.success = true;
        return result;
    } else if (ret == AVERROR(EAGAIN)) {
        result.success = false;
        return result;
    } else if (ret == AVERROR_EOF) {
        result.reached_eof = true;
        return result;
    } else {
        result.error_message = "Receive frame error: " + ffmpegErrorString(ret);
        return result;
    }
}

SingleFrameResult VideoDecoder::flushDecoder() {
    SingleFrameResult result{false, false, ""};

    if (!is_open_) {
        result.error_message = "Decoder not open";
        return result;
    }

    // Send null packet to signal EOF
    int ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        result.error_message = "Flush send error: " + ffmpegErrorString(ret);
        return result;
    }

    // Try to receive a buffered frame
    ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
    if (ret == 0) {
        av_frame_unref(frame_.get());
        result.success = true;
        return result;
    } else if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        result.reached_eof = true;
        return result;
    } else {
        result.error_message = "Flush receive error: " + ffmpegErrorString(ret);
        return result;
    }
}

} // namespace video_bench
