#include "utils/csv_exporter.hpp"
#include <fstream>

namespace video_bench {

bool CsvExporter::exportToFile(const BenchmarkResult& result,
                               const std::string& path,
                               std::string& error) {
    std::ofstream file(path);
    if (!file.is_open()) {
        error = "Failed to open CSV file: " + path;
        return false;
    }

    file << "stream_count,avg_fps,min_fps,max_fps,cpu_usage,memory_mb,"
            "fps_passed,cpu_passed,passed\n";

    for (const auto& test : result.test_results) {
        file << test.stream_count << ","
             << test.fps_per_stream << ","
             << test.min_fps << ","
             << test.max_fps << ","
             << test.cpu_usage << ","
             << test.memory_usage_mb << ","
             << (test.fps_passed ? "true" : "false") << ","
             << (test.cpu_passed ? "true" : "false") << ","
             << (test.passed ? "true" : "false") << "\n";
    }

    if (!file.good()) {
        error = "Failed to write CSV file: " + path;
        return false;
    }

    return true;
}

} // namespace video_bench
