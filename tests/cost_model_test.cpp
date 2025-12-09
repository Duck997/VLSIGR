#include <gtest/gtest.h>

#include "router/cost_model.hpp"
#include "router/grid_graph.hpp"

using namespace vlsigr;

TEST(CostModel, OverflowIncreasesCost) {
    CostModel cm(0);
    Edge e(1);
    e.demand = 0;
    double c0 = cm.calc_cost(e);
    e.demand = 1;  // demand+1 = 2 => overflow by 1
    double c1 = cm.calc_cost(e);
    EXPECT_LT(c0, c1);
}

TEST(CostModel, BuildCostOnGrid) {
    CostModel cm(1);
    GridGraph<Edge> g;
    g.init(2, 2, Edge(1), Edge(1));
    cm.build_cost(g);
    // all edges should have finite cost, and horizontal/vertical same cap so close values
    double vcost = g.at(0, 0, false).cost;
    double hcost = g.at(0, 0, true).cost;
    EXPECT_GT(vcost, 0.0);
    EXPECT_GT(hcost, 0.0);
    EXPECT_NEAR(vcost, hcost, 1e-6);
}


