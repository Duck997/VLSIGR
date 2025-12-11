#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "router/ispd_data.hpp"

namespace vlsigr {

enum class Mode : std::uint8_t {
    BALANCED = 0,
    CONGESTION = 1,
    WIRELENGTH = 2,
};

struct RoutingResults {
    const IspdData* data = nullptr;
};

struct PerformanceMetrics {
    double runtime_sec = 0.0;   
    int total_overflow = -1;
    int max_overflow = -1;
    long long wirelength_2d = -1;
    long long wirelength_total = -1;
    long long total_vias = -1;
};

class GlobalRouter {
public:
    GlobalRouter() = default;

    void load_ispd_benchmark(const std::string& gr_path);

    void init(IspdData data);
    void setMode(Mode m);
    void enableAdaptiveScoring(bool on);
    void enableHUMOptimization(bool on);

    void route(const std::string& la_output = "");

    const RoutingResults& getResults() const { return results_; }
    const PerformanceMetrics& getPerformanceMetrics() const { return metrics_; }

    void cleanup();

    const IspdData& data() const { return data_; }
    IspdData& mutable_data() { return data_; }

private:
    IspdData data_{};
    bool loaded_ = false;

    Mode mode_ = Mode::BALANCED;
    bool adaptive_scoring_ = true;
    bool hum_ = true;

    RoutingResults results_{};
    PerformanceMetrics metrics_{};
};

PerformanceMetrics route_ispd_file(const std::string& gr_path, const std::string& la_output = "");

}  // namespace vlsigr

namespace vlsigr::draw {
struct DrawOptions;
}  // namespace vlsigr::draw

// ---------------------------------------------------------------------------
// Proposal compatibility layer (compat with published examples)
// ---------------------------------------------------------------------------

namespace ISPDParser {
using ispdData = ::vlsigr::IspdData;

// Caller owns the returned pointer.
ispdData* parse_file(const std::string& path);
}  // namespace ISPDParser

namespace VLSIGR {
using Mode = ::vlsigr::Mode;
using PerformanceMetrics = ::vlsigr::PerformanceMetrics;

struct Results {
    const ISPDParser::ispdData* data = nullptr;
};

class GlobalRouting {
public:
    GlobalRouting() = default;

    void init(ISPDParser::ispdData& data);

    void setMode(Mode m);
    void enableAdaptiveScoring(bool on);
    void enableHUMOptimization(bool on);

    // Route using the currently loaded benchmark.
    void route();

    Results getResults() const { return results_; }
    PerformanceMetrics getPerformanceMetrics() const { return metrics_; }

    void cleanup();

private:
    ::vlsigr::GlobalRouter impl_{};
    Results results_{};
    PerformanceMetrics metrics_{};
};

class Visualization {
public:
    // Minimal net-colored visualization in PPM (P3).
    void generateMap(const ISPDParser::ispdData* data,
                     const Results& results,
                     const std::string& out_ppm) const;

    // Advanced overload: expose the full draw.cpp feature set to API callers.
    // - out_ppm overrides opt.out_ppm if non-empty.
    void generateMap(const ISPDParser::ispdData* data,
                     const Results& results,
                     const std::string& out_ppm,
                     const ::vlsigr::draw::DrawOptions& opt) const;
};

}  // namespace VLSIGR


