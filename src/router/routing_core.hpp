#pragma once

#include "router/ispd_data.hpp"
#include "router/grid_graph.hpp"
#include "router/cost_model.hpp"

namespace vlsigr {

class RoutingCore {
public:
    RoutingCore() = default;

    // Build grid capacities and run a simple L-shape preroute for all nets.
    void preroute(IspdData& data);

    // Access the grid (for future routing iterations).
    GridGraph<Edge>& grid() { return grid_; }
    const GridGraph<Edge>& grid() const { return grid_; }

private:
    GridGraph<Edge> grid_;
    CostModel cost_model_{0};
};

}  // namespace vlsigr



