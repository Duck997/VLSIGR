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

    // Single pass: find overflowed twopins, ripup, reroute, place, rebuild cost.
    void ripup_place_once(IspdData& data);

    struct OverflowStats {
        int tot = 0;
        int mx = 0;
        int wl = 0;
    };
    // Compute overflow stats and update history.
    OverflowStats check_overflow(IspdData& data);

    void set_selcost(int sel) { selcost_ = sel; cost_model_.set_selcost(sel); }

    // Multi-iteration routing loop (simple): run ripup_place_once until overflow not decreasing or iter cap.
    void route_iterate(IspdData& data, int max_iter = 10);

private:
    GridGraph<Edge> grid_;
    CostModel cost_model_{0};
    int selcost_ = 0;

    void place(TwoPin& tp);
    void ripup(TwoPin& tp);
    bool twopin_overflow(const TwoPin& tp) const;
    void route_twopin(TwoPin& tp);
    void mark_overflow(IspdData& data);
    void sort_twopins(IspdData& data);
    double score_twopin(const TwoPin& tp) const;
    double score_net(const Net& net) const;
    int hpwl(const TwoPin& tp) const;
};

}  // namespace vlsigr



