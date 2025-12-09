#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#include "router/ispd_data.hpp"
#include "router/grid_graph.hpp"
#include "router/utils.hpp"

using namespace vlsigr;

TEST(Parser, SmallMinimal) {
    // Minimal 2x2 grid, 1 layer, 1 net with 2 pins.
    std::string input = R"(grid 2 2 1
vertical capacity 10
horizontal capacity 20
minimum width 1
minimum spacing 1
via spacing 1
0 0 10 10
num net 1
net0 0 2 1
0 0 1
10 10 1
0
)";
    std::istringstream iss(input);
    auto data = parse_ispd(iss);
    EXPECT_EQ(data.numXGrid, 2);
    EXPECT_EQ(data.numYGrid, 2);
    EXPECT_EQ(data.numLayer, 1);
    ASSERT_EQ(data.nets.size(), 1u);
    const auto& net = data.nets[0];
    EXPECT_EQ(net.name, "net0");
    EXPECT_EQ(net.numPins, 2);
    ASSERT_EQ(net.pins.size(), 2u);
    EXPECT_EQ(std::get<0>(net.pins[0]), 0);
    EXPECT_EQ(std::get<1>(net.pins[0]), 0);
    EXPECT_EQ(std::get<2>(net.pins[0]), 1);
}

TEST(Parser, OfficialStyleSnippet) {
    // A tiny snippet that matches ISPD 2008 field order (headers + 2 nets + 1 cap adj).
    std::string input = R"(grid 3 2 1
vertical capacity 4
horizontal capacity 5
minimum width 1
minimum spacing 2
via spacing 3
0 0 10 10
num net 2
n1 1 2 1
0 0 1
10 0 1
n2 2 2 1
0 10 1
10 10 1
1
0 0 1 1 0 1 2
)";
    std::istringstream iss(input);
    auto data = parse_ispd(iss);
    EXPECT_EQ(data.numXGrid, 3);
    EXPECT_EQ(data.numYGrid, 2);
    EXPECT_EQ(data.numLayer, 1);
    ASSERT_EQ(data.verticalCapacity.size(), 1u);
    ASSERT_EQ(data.horizontalCapacity.size(), 1u);
    EXPECT_EQ(data.verticalCapacity[0], 4);
    EXPECT_EQ(data.horizontalCapacity[0], 5);

    ASSERT_EQ(data.nets.size(), 2u);
    EXPECT_EQ(data.nets[0].name, "n1");
    EXPECT_EQ(data.nets[1].name, "n2");
    EXPECT_EQ(data.nets[0].pins.size(), 2u);
    EXPECT_EQ(std::get<0>(data.nets[0].pins[1]), 10);

    EXPECT_EQ(data.numCapacityAdj, 1);
    ASSERT_EQ(data.capacityAdjs.size(), 1u);
    auto adj = data.capacityAdjs[0];
    EXPECT_EQ(std::get<0>(adj.grid1), 0);
    EXPECT_EQ(std::get<1>(adj.grid1), 0);
    EXPECT_EQ(std::get<2>(adj.grid1), 1);
    EXPECT_EQ(adj.reducedCapacityLevel, 2);
}

TEST(GridGraph, Indexing) {
    struct Edge { int v = 0; };
    GridGraph<Edge> g;
    g.init(3, 2, Edge{1}, Edge{2});  // width=3, height=2
    // vertical edges count: 3*(2-1)=3, horizontal: (3-1)*2=4
    EXPECT_EQ(g.width(), 3u);
    EXPECT_EQ(g.height(), 2u);
    EXPECT_EQ(g.at(0, 0, false).v, 1);
    EXPECT_EQ(g.at(1, 0, false).v, 1);
    EXPECT_EQ(g.at(2, 0, false).v, 1);
    EXPECT_EQ(g.at(0, 0, true).v, 2);
    EXPECT_EQ(g.at(1, 0, true).v, 2);
    EXPECT_EQ(g.at(0, 1, true).v, 2);
    EXPECT_EQ(g.at(1, 1, true).v, 2);
}

TEST(Utils, SignAndAverage) {
    EXPECT_EQ(sign(-5), -1);
    EXPECT_EQ(sign(0), 0);
    EXPECT_EQ(sign(7), 1);
    std::vector<int> v{1, 2, 3, 4};
    EXPECT_EQ(average(v), 10 / 4);
}


