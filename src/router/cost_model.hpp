#pragma once

// Cost model interfaces (edge cost, scoring, overflow penalties).
#pragma once

#include <cmath>

#include "router/grid_graph.hpp"

namespace vlsigr {

struct Edge {
    int cap;
    int demand;
    int he;   // history
    int of;   // overflow count
    int used;
    double cost;
    explicit Edge(int c = 0): cap(c), demand(0), he(1), of(0), used(0), cost(1.0) {}
    bool overflow() const { return cap < demand; }
};

class CostModel {
public:
    static constexpr int COSTSZ  = 1024;
    static constexpr int COSTOFF = 256;

    explicit CostModel(int sel = 0): selcost(sel) { build_cost_pe(); }

    void set_selcost(int sel) { selcost = sel; build_cost_pe(); }

    // Calculate cost for one edge.
    double calc_cost(const Edge& e) const;

    // Recompute cost for all edges in the grid.
    template<typename Grid>
    void build_cost(Grid& grid) {
        for (auto it = grid.begin(); it != grid.end(); ++it)
            it->cost = calc_cost(*it);
    }

private:
    int selcost;  // 0: mild, 1: steeper, 2: aggressive
    double cost_pe[COSTSZ];

    void build_cost_pe();
    inline double get_cost_pe(int of) const {
        int i = of + COSTOFF;
        if (i <= 0) return cost_pe[0];
        if (i >= COSTSZ) return cost_pe[COSTSZ-1];
        return cost_pe[i];
    }
};

}  // namespace vlsigr



