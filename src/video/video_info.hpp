#ifndef VIDEO_INFO_HPP
#define VIDEO_INFO_HPP

#include <string>
#include <optional>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace video_bench {

// Supported video codecs
enum class VideoCodec {
    H264,
    H265,
    VP9,
    AV1,
    Unknown
};

// Video file information
struct VideoInfo {
    std::string file_path;
    std::string codec_name;
    VideoCodec codec_type;
    int width;
    int height;
    double fps;
    double duration_seconds;
    int64_t total_frames;
    int video_stream_index = -1;
    bool is_live_stream = false;  // True for RTSP and other live sources

    // Format resolution as string (e.g., "1080p", "4K")
    std::string getResolutionString() const;

    // Check if codec is supported
    bool isCodecSupported() const;
};

// Analyze a video file and extract its information
class VideoAnalyzer {
public:
    // Analyze video file and return info, or nullopt on error
    static std::optional<VideoInfo> analyze(const std::string& file_path,
                                            std::string& error_message);

private:
    static VideoCodec codecIdToType(AVCodecID codec_id);
    static std::string codecIdToName(AVCodecID codec_id);
};

} // namespace video_bench

#endif // VIDEO_INFO_HPP
