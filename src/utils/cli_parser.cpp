#include "utils/cli_parser.hpp"
#include "video_bench/version.hpp"
#include <iostream>
#include <charconv>
#include <filesystem>

namespace video_bench {

std::optional<int> CliParser::parseInteger(const std::string& str) {
    int value;
    auto result = std::from_chars(str.data(), str.data() + str.size(), value);
    if (result.ec == std::errc() && result.ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

std::optional<double> CliParser::parseDouble(const std::string& str) {
    try {
        size_t pos;
        double value = std::stod(str, &pos);
        if (pos == str.size()) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

CliParseResult CliParser::parse(int argc, char* argv[]) {
    CliParseResult result;
    result.success = true;
    result.show_help = false;
    result.show_version = false;

    std::vector<std::string> args(argv, argv + argc);
    std::string video_path;

    for (size_t i = 1; i < args.size(); i++) {
        const std::string& arg = args[i];

        if (arg == "-h" || arg == "--help") {
            result.show_help = true;
            return result;
        }

        if (arg == "-v" || arg == "--version") {
            result.show_version = true;
            return result;
        }

        if (arg == "--max-streams" || arg == "-m") {
            if (i + 1 >= args.size()) {
                result.success = false;
                result.error_message = "Missing value for --max-streams";
                return result;
            }
            auto value = parseInteger(args[++i]);
            if (!value || *value <= 0) {
                result.success = false;
                result.error_message = "Invalid value for --max-streams: must be a positive integer";
                return result;
            }
            result.config.max_streams = *value;
            continue;
        }

        if (arg == "--target-fps" || arg == "-f") {
            if (i + 1 >= args.size()) {
                result.success = false;
                result.error_message = "Missing value for --target-fps";
                return result;
            }
            auto value = parseDouble(args[++i]);
            if (!value || *value <= 0) {
                result.success = false;
                result.error_message = "Invalid value for --target-fps: must be a positive number";
                return result;
            }
            result.config.target_fps = *value;
            continue;
        }

        if (arg == "--log-file" || arg == "-l") {
            if (i + 1 >= args.size()) {
                result.success = false;
                result.error_message = "Missing value for --log-file";
                return result;
            }
            result.config.log_file = args[++i];
            continue;
        }

        if (arg == "--csv-file" || arg == "-c") {
            if (i + 1 >= args.size()) {
                result.success = false;
                result.error_message = "Missing value for --csv-file";
                return result;
            }
            result.config.csv_file = args[++i];
            continue;
        }

        if (arg[0] == '-') {
            result.success = false;
            result.error_message = "Unknown option: " + arg;
            return result;
        }

        // Positional argument - video path
        if (video_path.empty()) {
            video_path = arg;
        } else {
            result.success = false;
            result.error_message = "Too many arguments";
            return result;
        }
    }

    if (video_path.empty()) {
        result.success = false;
        result.error_message = "Missing video file path or RTSP URL";
        return result;
    }

    // Check if it's an RTSP URL or file
    bool is_rtsp = (video_path.find("rtsp://") == 0 || video_path.find("rtsps://") == 0);

    if (!is_rtsp && !std::filesystem::exists(video_path)) {
        result.success = false;
        result.error_message = "File not found: " + video_path;
        return result;
    }

    result.config.video_path = video_path;
    return result;
}

void CliParser::printUsage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] <video_source>\n"
              << "\n"
              << "Video decoding benchmark tool - measures concurrent decoding capacity\n"
              << "\n"
              << "Arguments:\n"
              << "  <video_source>         Path to video file or RTSP URL\n"
              << "\n"
              << "Options:\n"
              << "  -m, --max-streams N    Maximum number of streams to test (default: CPU thread count)\n"
              << "  -f, --target-fps FPS   Target FPS for real-time threshold (default: video's native FPS)\n"
              << "  -l, --log-file PATH    Log file path (default: video-benchmark.log)\n"
              << "  -c, --csv-file PATH    Export results to CSV file\n"
              << "  -h, --help             Show this help message\n"
              << "  -v, --version          Show version information\n"
              << "\n"
              << "Supported codecs: H.264, H.265/HEVC, VP9, AV1\n"
              << "Supported inputs: Local files, RTSP streams (rtsp://)\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " video.mp4\n"
              << "  " << program_name << " --max-streams 8 video.mp4\n"
              << "  " << program_name << " rtsp://192.168.1.100:554/stream\n"
              << "  " << program_name << " -f 30 -m 4 rtsp://camera.local/live\n";
}

void CliParser::printVersion() {
    std::cout << PROGRAM_NAME << " version " << VERSION << "\n";
}

} // namespace video_bench
