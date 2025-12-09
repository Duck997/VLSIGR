#pragma once

#include <functional>
#include <queue>
#include <vector>

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
    void ripup_place_once(IspdData& data, const std::function<void(TwoPin&)>& route_func);

    struct OverflowStats {
        int tot = 0;
        int mx = 0;
        int wl = 0;
    };
    // Compute overflow stats and update history.
    OverflowStats check_overflow(IspdData& data);

    void set_selcost(int sel) { selcost_ = sel; cost_model_.set_selcost(sel); }

    // Multi-iteration routing loop (simple): run ripup_place_once until overflow not decreasing or iter cap.
    void route_iterate(IspdData& data, int max_iter = 10,
                       const std::function<void(TwoPin&)>& route_func = {});

    // Multi-stage pipeline (approximate legacy): Zshape -> Monotonic -> HUM
    void route_pipeline(IspdData& data);

private:
    // Flow steps
    void construct_2D_grid_graph(IspdData& data);
    void net_decomposition(IspdData& data);

    GridGraph<Edge> grid_;
    CostModel cost_model_{0};
    int selcost_ = 0;

    void place(TwoPin& tp);
    void ripup(TwoPin& tp);
    bool twopin_overflow(const TwoPin& tp) const;
    void route_twopin_default(TwoPin& tp);
    void route_twopin_monotonic(TwoPin& tp);
    void route_twopin_lshape(TwoPin& tp);
    void route_twopin_zshape(TwoPin& tp);
    void route_twopin_hum(TwoPin& tp);
    void mark_overflow(IspdData& data);
    void sort_twopins(IspdData& data);
    double score_twopin(const TwoPin& tp) const;
    double score_net(const Net& net) const;
    int hpwl(const TwoPin& tp) const;
    void refine_wirelength(IspdData& data, const std::function<void(TwoPin&)>& route_func, int iter);

    // Cost maintenance helpers (align with legacy GlobalRouting)
    void del_cost(TwoPin& tp);
    void del_cost(Net& net);
    void add_cost(TwoPin& tp);
    void add_cost(Net& net);
};

}  // namespace vlsigr



