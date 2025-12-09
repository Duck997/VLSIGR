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

    // Single pass: find overflowed twopins, ripup, reroute (monotonic), place, rebuild cost.
    void ripup_place_once(IspdData& data);
    // Compute total overflow (sum of (demand - cap) over edges, clamped at >0).
    int check_overflow() const;

private:
    GridGraph<Edge> grid_;
    CostModel cost_model_{0};

    void place(TwoPin& tp);
    void ripup(TwoPin& tp);
    bool twopin_overflow(const TwoPin& tp) const;
    void route_twopin(TwoPin& tp);
    void mark_overflow(IspdData& data);
    void sort_twopins(IspdData& data);
};

}  // namespace vlsigr



