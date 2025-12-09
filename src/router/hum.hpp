#pragma once

// HUM-specific logic: bounding box expansion, cost grids, VMR/HMR sweeps.
#include <cstddef>

#include "router/ispd_data.hpp"
#include "router/grid_graph.hpp"
#include "router/cost_model.hpp"

namespace vlsigr::hum {

// Route a two-pin using a simplified HUM-like box expansion and cost DP.
// width/height are grid dimensions.
void HUM(TwoPin& tp, GridGraph<Edge>& grid, CostModel& cm, std::size_t width, std::size_t height);

}  // namespace vlsigr::hum



