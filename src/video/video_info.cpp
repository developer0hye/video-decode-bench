#include "video/video_info.hpp"
#include <cmath>

namespace video_bench {

std::string VideoInfo::getResolutionString() const {
    if (height >= 2160) {
        return "4K";
    } else if (height >= 1440) {
        return "1440p";
    } else if (height >= 1080) {
        return "1080p";
    } else if (height >= 720) {
        return "720p";
    } else if (height >= 480) {
        return "480p";
    } else {
        return std::to_string(height) + "p";
    }
}

bool VideoInfo::isCodecSupported() const {
    return codec_type != VideoCodec::Unknown;
}

VideoCodec VideoAnalyzer::codecIdToType(AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_H264:
            return VideoCodec::H264;
        case AV_CODEC_ID_HEVC:
            return VideoCodec::H265;
        case AV_CODEC_ID_VP9:
            return VideoCodec::VP9;
        case AV_CODEC_ID_AV1:
            return VideoCodec::AV1;
        default:
            return VideoCodec::Unknown;
    }
}

std::string VideoAnalyzer::codecIdToName(AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_H264:
            return "H.264";
        case AV_CODEC_ID_HEVC:
            return "H.265";
        case AV_CODEC_ID_VP9:
            return "VP9";
        case AV_CODEC_ID_AV1:
            return "AV1";
        default:
            return "Unknown";
    }
}

std::optional<VideoInfo> VideoAnalyzer::analyze(const std::string& file_path,
                                                 std::string& error_message) {
    AVFormatContext* format_ctx = nullptr;

    // Open input file
    int ret = avformat_open_input(&format_ctx, file_path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Failed to open file: " + std::string(err_buf);
        return std::nullopt;
    }

    // RAII cleanup for format context
    auto format_guard = std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)>(
        format_ctx,
        [](AVFormatContext* ctx) { avformat_close_input(&ctx); }
    );

    // Find stream info
    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        error_message = "Failed to find stream info: " + std::string(err_buf);
        return std::nullopt;
    }

    // Find video stream
    int video_stream_index = -1;
    AVCodecParameters* codec_params = nullptr;

    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = static_cast<int>(i);
            codec_params = format_ctx->streams[i]->codecpar;
            break;
        }
    }

    if (video_stream_index < 0) {
        error_message = "No video stream found in file";
        return std::nullopt;
    }

    AVStream* video_stream = format_ctx->streams[video_stream_index];

    // Calculate FPS
    double fps = 0.0;
    if (video_stream->avg_frame_rate.den != 0) {
        fps = av_q2d(video_stream->avg_frame_rate);
    } else if (video_stream->r_frame_rate.den != 0) {
        fps = av_q2d(video_stream->r_frame_rate);
    }

    if (fps <= 0.0) {
        error_message = "Could not determine video frame rate";
        return std::nullopt;
    }

    // Calculate duration
    double duration = 0.0;
    if (format_ctx->duration != AV_NOPTS_VALUE) {
        duration = static_cast<double>(format_ctx->duration) / AV_TIME_BASE;
    } else if (video_stream->duration != AV_NOPTS_VALUE) {
        duration = static_cast<double>(video_stream->duration) *
                   av_q2d(video_stream->time_base);
    }

    // Calculate total frames
    int64_t total_frames = video_stream->nb_frames;
    if (total_frames <= 0 && duration > 0) {
        total_frames = static_cast<int64_t>(std::round(duration * fps));
    }

    VideoInfo info;
    info.file_path = file_path;
    info.codec_type = codecIdToType(codec_params->codec_id);
    info.codec_name = codecIdToName(codec_params->codec_id);
    info.width = codec_params->width;
    info.height = codec_params->height;
    info.fps = fps;
    info.duration_seconds = duration;
    info.total_frames = total_frames;
    info.video_stream_index = video_stream_index;

    return info;
}

} // namespace video_bench
