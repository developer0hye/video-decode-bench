#include "utils/cli_parser.hpp"
#include "utils/output_formatter.hpp"
#include "utils/csv_exporter.hpp"
#include "utils/logger.hpp"
#include "benchmark/benchmark_runner.hpp"
#include "video/video_info.hpp"
#include "monitor/system_info.hpp"
#include "monitor/memory_monitor.hpp"
#include <iostream>

using namespace video_bench;

int main(int argc, char* argv[]) {
    // Parse command line arguments first to get log file path
    auto parse_result = CliParser::parse(argc, argv);

    // Initialize logger with configured or default path
    struct LoggerShutdownGuard {
        ~LoggerShutdownGuard() {
            Logger::shutdown();
        }
    } logger_shutdown_guard;
    (void)logger_shutdown_guard;

    const std::string log_file_path = parse_result.config.log_file.value_or(
        Logger::defaultLogFilePath());
    std::string logger_error;
    if (!Logger::initialize(log_file_path, logger_error)) {
        std::cerr << "Warning: Failed to initialize log file '" << log_file_path
                  << "': " << logger_error << "\n";
    } else {
        Logger::info("Log file: " + log_file_path);
        std::string cmdline;
        for (int i = 0; i < argc; i++) {
            if (i > 0) cmdline += ' ';
            cmdline += argv[i];
        }
        Logger::info("Command: " + cmdline);
    }

    if (!parse_result.success) {
        OutputFormatter::printError(parse_result.error_message);
        std::string help_hint = "Try '" + std::string(argv[0]) + " --help' for more information.";
        std::cerr << help_hint << "\n";
        Logger::error(help_hint);
        return 1;
    }

    if (parse_result.show_help) {
        CliParser::printUsage(argv[0]);
        return 0;
    }

    if (parse_result.show_version) {
        CliParser::printVersion();
        return 0;
    }

    // Analyze video first to print header before benchmark starts
    std::string error;
    auto video_info = VideoAnalyzer::analyze(parse_result.config.video_path, error);
    if (!video_info) {
        OutputFormatter::printError(error);
        return 1;
    }

    // Check codec support
    if (!video_info->isCodecSupported()) {
        OutputFormatter::printError("Unsupported codec: " + video_info->codec_name);
        return 1;
    }

    // Build a partial result for header printing
    BenchmarkResult header_info;
    header_info.cpu_name = SystemInfo::getCpuName();
    header_info.thread_count = SystemInfo::getThreadCount();
    auto mem_monitor = MemoryMonitor::create();
    header_info.total_system_memory_mb = mem_monitor->getTotalSystemMemoryMB();
    header_info.video_path = parse_result.config.video_path;
    header_info.video_resolution = video_info->getResolutionString();
    header_info.codec_name = video_info->codec_name;
    header_info.video_fps = video_info->fps;
    header_info.is_live_stream = video_info->is_live_stream;

    // Print header
    OutputFormatter::printHeader(header_info);
    OutputFormatter::printTestingStart();

    // Run benchmark
    BenchmarkRunner runner(parse_result.config, *video_info);

    auto result = runner.run([](const StreamTestResult& test_result) {
        OutputFormatter::printTestResult(test_result);
    });

    if (!result.success) {
        OutputFormatter::printError(result.error_message);
        return 1;
    }

    // Print summary
    OutputFormatter::printSummary(result);

    // Export CSV if requested
    if (parse_result.config.csv_file) {
        std::string csv_error;
        if (!CsvExporter::exportToFile(result, *parse_result.config.csv_file, csv_error)) {
            OutputFormatter::printError(csv_error);
            return 1;
        }
        Logger::info("CSV results exported to: " + *parse_result.config.csv_file);
    }

    return 0;
}
