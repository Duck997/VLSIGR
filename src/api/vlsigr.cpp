#include "api/vlsigr.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "router/routing_core.hpp"
#include "router/layer_assignment.hpp"
#include "router/utils.hpp"
#include "tools/draw_api.hpp"

namespace vlsigr {

void GlobalRouter::load_ispd_benchmark(const std::string& gr_path) {
    data_ = parse_ispd_file(gr_path);
    loaded_ = true;
    results_.data = &data_;
}

void GlobalRouter::init(IspdData data) {
    data_ = std::move(data);
    loaded_ = true;
    results_.data = &data_;
}

void GlobalRouter::setMode(Mode m) {
    mode_ = m;
}

void GlobalRouter::enableAdaptiveScoring(bool on) {
    adaptive_scoring_ = on;
}

void GlobalRouter::enableHUMOptimization(bool on) {
    hum_ = on;
}

void GlobalRouter::cleanup() {
    data_ = IspdData{};
    loaded_ = false;
    results_ = RoutingResults{};
    metrics_ = PerformanceMetrics{};
}

void GlobalRouter::route(const std::string& la_output) {
    if (!loaded_) {
        throw std::runtime_error("GlobalRouter: benchmark not loaded. Call load_ispd_benchmark() or init() first.");
    }

    auto t0 = std::chrono::steady_clock::now();

    (void)mode_;
    (void)adaptive_scoring_;
    (void)hum_;

    RoutingCore core;
    try {
        core.route(data_, false);
    } catch (bool /*done*/) {
        // Converged early, OK.
    }

    metrics_ = PerformanceMetrics{};
    metrics_.runtime_sec = vlsigr::sec_since(t0);
    results_.data = &data_;

    // If requested, run LayerAssignment and use its statistics (best available metrics).
    if (!la_output.empty()) {
        auto la = run_layer_assignment(data_, la_output, true);
        metrics_.total_overflow = la.totalOF;
        metrics_.max_overflow = la.maxOF;
        metrics_.total_vias = la.totalVia;
        metrics_.wirelength_2d = la.wlen2D;
        metrics_.wirelength_total = la.totalWL;
        return;
    }

    // Otherwise, collect lightweight approximations from 2D routed paths.
    long long wl2d = 0;
    int of_tp = 0;
    for (const auto& net : data_.nets) {
        for (const auto& tp : net.twopin) {
            wl2d += static_cast<long long>(tp.path.size());
            if (tp.overflow) of_tp++;
        }
    }
    metrics_.wirelength_2d = wl2d;
    metrics_.wirelength_total = wl2d;
    metrics_.total_overflow = of_tp;
    metrics_.max_overflow = -1;
    metrics_.total_vias = -1;
}

PerformanceMetrics route_ispd_file(const std::string& gr_path, const std::string& la_output) {
    GlobalRouter r;
    r.load_ispd_benchmark(gr_path);
    r.route(la_output);
    return r.getPerformanceMetrics();
}

}  // namespace vlsigr

// ---------------------------------------------------------------------------
// Proposal compatibility layer implementations
// ---------------------------------------------------------------------------

namespace ISPDParser {

ispdData* parse_file(const std::string& path) {
    return new ispdData(::vlsigr::parse_ispd_file(path));
}

}  // namespace ISPDParser

namespace VLSIGR {

void GlobalRouting::init(ISPDParser::ispdData& data) {
    impl_.init(data);
    results_.data = &data;
}

void GlobalRouting::setMode(Mode m) {
    impl_.setMode(m);
}

void GlobalRouting::enableAdaptiveScoring(bool on) {
    impl_.enableAdaptiveScoring(on);
}

void GlobalRouting::enableHUMOptimization(bool on) {
    impl_.enableHUMOptimization(on);
}

void GlobalRouting::route() {
    impl_.route("");
    metrics_ = impl_.getPerformanceMetrics();
    results_.data = &impl_.data();
}

void GlobalRouting::cleanup() {
    impl_.cleanup();
    results_ = Results{};
    metrics_ = PerformanceMetrics{};
}

void Visualization::generateMap(const ISPDParser::ispdData* data,
                                const Results& results,
                                const std::string& out_ppm) const {
    const auto* d = results.data ? results.data : data;
    if (!d) throw std::runtime_error("Visualization::generateMap: null data/results");
    vlsigr::draw::DrawOptions opt;
    opt.out_ppm = out_ppm;
    opt.scale = 1;
    vlsigr::draw::render_from_data(*d, opt);
}

void Visualization::generateMap(const ISPDParser::ispdData* data,
                                const Results& results,
                                const std::string& out_ppm,
                                const ::vlsigr::draw::DrawOptions& opt) const {
    const auto* d = results.data ? results.data : data;
    if (!d) throw std::runtime_error("Visualization::generateMap: null data/results");

    ::vlsigr::draw::DrawOptions o = opt;
    if (!out_ppm.empty()) o.out_ppm = out_ppm;
    vlsigr::draw::render_from_data(*d, o);
}

}  // namespace VLSIGR


