#ifndef VIDEO_DECODER_HPP
#define VIDEO_DECODER_HPP

#include "utils/ffmpeg_utils.hpp"
#include <string>
#include <cstdint>

namespace video_bench {

// Result of a decode operation
struct DecodeResult {
    int64_t frames_decoded;
    bool reached_eof;
    std::string error_message;
};

// Result of decoding a single frame
struct SingleFrameResult {
    bool success;              // Frame decoded successfully
    bool reached_eof;          // Reached end of file (seek performed)
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
    // thread_count: number of decoder threads (1 = single-threaded, 0 = auto)
    // is_live_stream: true for RTSP and other non-seekable sources
    bool open(const std::string& file_path, std::string& error_message,
              int thread_count = 1, bool is_live_stream = false);

    // Initialize codec context from external codec parameters (no file open)
    // Used in pipeline mode where PacketReader owns the format context
    bool initFromParams(const AVCodecParameters* codec_params,
                        std::string& error_message,
                        int thread_count = 1,
                        bool is_live_stream = false);

    // Check if decoder is open
    bool isOpen() const;

    // Decode frames for a specified duration (in seconds)
    // Returns number of frames decoded
    DecodeResult decodeFor(double duration_seconds);

    // Decode exactly one video frame
    // Returns immediately after decoding one frame
    SingleFrameResult decodeOneFrame();

    // Decode one frame from an external packet (for pipeline mode)
    // Caller retains ownership of packet
    SingleFrameResult decodeFromPacket(AVPacket* packet);

    // Flush decoder to get remaining buffered frames (call at EOF)
    // Returns true if a frame was decoded
    SingleFrameResult flushDecoder();

    // Seek to the beginning of the video
    bool seekToStart();

    // Get video stream index
    int getVideoStreamIndex() const { return video_stream_index_; }

private:
    // Decode all available frames from current packet
    // Returns frames decoded. Sets error_out on decode failure.
    int64_t decodePacket(std::string* error_out);

    // Handle EOF: drain decoder, seek or report based on stream type
    SingleFrameResult handleEof();

    UniqueAVFormatContext format_ctx_;
    UniqueAVCodecContext codec_ctx_;
    UniqueAVFrame frame_;
    UniqueAVPacket packet_;

    int video_stream_index_ = -1;
    bool is_open_ = false;
    bool is_live_stream_ = false;
};

} // namespace video_bench

#endif // VIDEO_DECODER_HPP
