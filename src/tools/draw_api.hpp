#pragma once

#include <string>

#include "router/ispd_data.hpp"

namespace vlsigr::draw {

struct DrawOptions {
    // Primary outputs
    std::string out_map;        // map.txt (optional)
    std::string out_ppm;        // main congestion PPM (optional)

    // Optional extra outputs (match draw CLI)
    std::string overflow_ppm;   // overflow mask PPM
    bool overflow_show_blockages = false; // if true, color blockages in overflow mask
    int overflow_x_size = 0;    // size (pixels) of the 'X' mark on overflow edges; 0 = auto
    std::string nets_ppm;       // net-colored PPM
    std::string layer_dir;      // per-layer PPM directory
    std::string stats_path;     // stats txt path

    int scale = 1;
};

// File-based render (same as the draw CLI, but callable).
void render_from_files(const std::string& input_gr,
                       const std::string& input_out,
                       const DrawOptions& opt);

// In-memory render (for API integration without output.txt).
void render_from_data(const vlsigr::IspdData& data,
                      const DrawOptions& opt);

// Backward-compatible helper (kept for convenience).
void generate_map_from_data(const vlsigr::IspdData& data,
                            const std::string& out_ppm,
                            int scale = 1);

}  // namespace vlsigr::draw


