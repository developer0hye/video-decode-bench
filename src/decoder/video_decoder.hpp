#ifndef VIDEO_DECODER_HPP
#define VIDEO_DECODER_HPP

#include <string>
#include <memory>
#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace video_bench {

// Custom deleter for AVFormatContext
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            avformat_close_input(&ctx);
        }
    }
};

// Custom deleter for AVCodecContext
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
};

// Custom deleter for AVFrame
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

// Custom deleter for AVPacket
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
};

// Result of a decode operation
struct DecodeResult {
    int64_t frames_decoded;
    bool reached_eof;
    std::string error_message;
};

// Single-threaded video decoder
// Each instance owns its own FFmpeg context for thread safety
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder() = default;

    // Non-copyable, movable
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    VideoDecoder(VideoDecoder&&) = default;
    VideoDecoder& operator=(VideoDecoder&&) = default;

    // Open a video file for decoding
    bool open(const std::string& file_path, std::string& error_message);

    // Check if decoder is open
    bool isOpen() const;

    // Decode frames for a specified duration (in seconds)
    // Returns number of frames decoded
    DecodeResult decodeFor(double duration_seconds);

    // Seek to the beginning of the video
    bool seekToStart();

    // Get video stream index
    int getVideoStreamIndex() const { return video_stream_index_; }

private:
    // Decode all available frames from current packet
    // Returns frames decoded. Sets error_out on decode failure.
    int64_t decodePacket(std::string* error_out);

    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> format_ctx_;
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codec_ctx_;
    std::unique_ptr<AVFrame, AVFrameDeleter> frame_;
    std::unique_ptr<AVPacket, AVPacketDeleter> packet_;

    int video_stream_index_ = -1;
    bool is_open_ = false;
};

} // namespace video_bench

#endif // VIDEO_DECODER_HPP
