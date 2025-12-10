#pragma once

// Layer assignment wrapper that bridges our IspdData to third_party LayerAssignment.

#include <string>

#include "router/ispd_data.hpp"

namespace vlsigr {

struct LayerAssignmentResult {
    int totalOF = 0;
    int maxOF = 0;
    int totalVia = 0;
    int wlen2D = 0;
    int via = 0;
    int totalWL = 0;  // wlen2D + via*1
};

// Run 3D layer assignment using third_party LayerAssignment.
// output_path empty -> skip file emission.
LayerAssignmentResult run_layer_assignment(IspdData& data,
                                           const std::string& output_path,
                                           bool print_to_screen);

}  // namespace vlsigr


