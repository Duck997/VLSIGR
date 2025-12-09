#include <gtest/gtest.h>
#include <string>
#include <sstream>

#include "router/routing_core.hpp"
#include "router/ispd_data.hpp"

using namespace vlsigr;

TEST(RoutingCore, PrerouteBuildsTwopinsAndGrid) {
    // Small ISPD-like input: 3x2 grid, 1 layer, 1 net with 2 pins.
    std::string input = R"(grid 3 2 1
vertical capacity 10
horizontal capacity 20
minimum width 1
minimum spacing 1
via spacing 1
0 0 10 10
num net 1
net0 0 2 1
0 0 1
20 10 1
0
)";
    std::istringstream iss(input);
    auto data = parse_ispd(iss);

    RoutingCore rc;
    rc.preroute(data);

    // Grid dimensions and capacities built
    EXPECT_EQ(rc.grid().width(), 3u);
    EXPECT_EQ(rc.grid().height(), 2u);
    EXPECT_EQ(rc.grid().at(0, 0, false).cap, 10); // vertical cap sum
    EXPECT_EQ(rc.grid().at(0, 0, true).cap, 20);  // horizontal cap sum

    ASSERT_EQ(data.nets.size(), 1u);
    const auto& net = data.nets[0];
    ASSERT_EQ(net.twopin.size(), 1u);
    const auto& tp = net.twopin[0];
    // Pins mapped to grid coordinates: (0,0,0) -> (2,1,0)
    int dx = std::abs(tp.from.x - tp.to.x);
    int dy = std::abs(tp.from.y - tp.to.y);
    EXPECT_EQ(tp.path.size(), static_cast<size_t>(dx + dy));

    // check_overflow should be zero
    EXPECT_EQ(rc.check_overflow(), 0);
}


