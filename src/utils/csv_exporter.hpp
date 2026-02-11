#ifndef CSV_EXPORTER_HPP
#define CSV_EXPORTER_HPP

#include "benchmark/benchmark_result.hpp"
#include <string>

namespace video_bench {

class CsvExporter {
public:
    static bool exportToFile(const BenchmarkResult& result,
                             const std::string& path,
                             std::string& error);
};

} // namespace video_bench

#endif // CSV_EXPORTER_HPP
