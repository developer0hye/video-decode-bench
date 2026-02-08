#ifndef PACKET_READER_HPP
#define PACKET_READER_HPP

#include "utils/ffmpeg_utils.hpp"
#include "decoder/packet_queue.hpp"
#include <string>
#include <atomic>

namespace video_bench {

// I/O-dedicated reader that reads packets from video source
// Runs in a separate thread to decouple I/O from decoding
class PacketReader {
public:
    PacketReader(const std::string& path,
                 PacketQueue& queue,
                 std::atomic<bool>& stop_flag,
                 bool is_live_stream);

    // Initialize the reader (open file/stream, find video stream)
    bool init(std::string& error_message);

    // Reader thread entry point
    void run();

    // Check if reader encountered an error
    bool hasError() const;

    // Get error message if hasError() is true
    std::string getError() const;

    // Get the discovered video stream index (valid after init())
    int getVideoStreamIndex() const;

    // Get codec parameters for the video stream (valid after init())
    const AVCodecParameters* getCodecParameters() const;

private:
    std::string path_;
    PacketQueue& queue_;
    std::atomic<bool>& stop_flag_;
    bool is_live_stream_;
    int video_stream_index_ = -1;

    UniqueAVFormatContext format_ctx_;
    UniqueAVPacket packet_;
    const AVCodecParameters* codec_params_ = nullptr;

    std::atomic<bool> has_error_{false};
    std::string error_message_;
};

} // namespace video_bench

#endif // PACKET_READER_HPP
