#include <gtest/gtest.h>

#include "router/hum.hpp"
#include "router/grid_graph.hpp"
#include "router/cost_model.hpp"
#include "router/patterns.hpp"

using namespace vlsigr;

namespace {
void place_path(const TwoPin& tp, GridGraph<Edge>& grid) {
    for (auto& rp : tp.path) {
        grid.at(rp.x, rp.y, rp.hori).demand += 1;
    }
}
}

TEST(HUM, RelievesOverflow) {
    // 3x3 grid with very low capacity and a pre-blocked monotonic corridor.
    GridGraph<Edge> grid;
    grid.init(3, 3, Edge(1), Edge(1));  // tight capacity
    CostModel cm(0);

    TwoPin tp;
    tp.from = Point(0, 0, 0);
    tp.to   = Point(2, 2, 0);

    // Pre-block the intuitive monotonic path along bottom row then up right edge:
    // (0,0)-(1,0)-(2,0)-(2,1)-(2,2)
    grid.at(0, 0, true).demand += 1; // (0,0)->(1,0)
    grid.at(1, 0, true).demand += 1; // (1,0)->(2,0)
    grid.at(2, 0, false).demand += 1; // (2,0)->(2,1)
    grid.at(2, 1, false).demand += 1; // (2,1)->(2,2)

    // First try the intuitive monotonic corridor and confirm it overflows:
    // (0,0)->(1,0)->(2,0)->(2,1)->(2,2)
    TwoPin monotonic_tp = tp;
    monotonic_tp.path = {
        RPoint(0, 0, true),
        RPoint(1, 0, true),
        RPoint(2, 0, false),
        RPoint(2, 1, false),
    };
    place_path(monotonic_tp, grid);
    bool mono_overflow = false;
    for (auto& rp : monotonic_tp.path) {
        if (grid.at(rp.x, rp.y, rp.hori).overflow()) { mono_overflow = true; break; }
    }
    EXPECT_TRUE(mono_overflow);
    // reset demands
    for (auto& rp : monotonic_tp.path) grid.at(rp.x, rp.y, rp.hori).demand -= 1;

    // Run HUM and expect no overflow on touched edges
    // IMPORTANT: rebuild cost after modifying demands, otherwise HUM won't "see" congestion.
    cm.build_cost(grid);
    hum::HUM(tp, grid, cm, grid.width(), grid.height());
    place_path(tp, grid);
    for (auto& rp : tp.path) {
        auto& e = grid.at(rp.x, rp.y, rp.hori);
        EXPECT_FALSE(e.overflow());
    }
}


