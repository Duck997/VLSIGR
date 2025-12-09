#include <gtest/gtest.h>

#include "router/ispd_data.hpp"
#include "router/patterns.hpp"

using namespace vlsigr;
using namespace vlsigr::patterns;

TEST(Patterns, Lshape) {
    TwoPin tp;
    tp.from.x = 0; tp.from.y = 0; tp.from.z = 0;
    tp.to.x   = 2; tp.to.y   = 1; tp.to.z   = 0;
    Lshape(tp);
    // Manhattan distance = 3 edges
    EXPECT_EQ(tp.path.size(), 3u);
}

TEST(Patterns, LshapeCostPreference) {
    // Prefer turning via (0,1)->(2,1) horizontally (cheap),
    // make vertical edge at x=1 expensive so path should not go via (1,0)->(1,1).
    TwoPin tp;
    tp.from.x = 0; tp.from.y = 0; tp.from.z = 0;
    tp.to.x   = 2; tp.to.y   = 2; tp.to.z   = 0;
    auto cost = [](int x, int y, bool hori) {
        if (!hori && x == 1 && y == 0) return 100.0;  // make this vertical edge expensive
        return 1.0;
    };
    Lshape(tp, cost);
    ASSERT_EQ(tp.path.size(), 4u);
    // Path should go (0,0)->(1,0)->(2,0)->(2,1)->(2,2) or via y=1 turn, but avoid x=1 vertical at y=0.
    // Ensure no vertical edge at (1,0)
    for (auto& e : tp.path) {
        ASSERT_FALSE(!e.hori && e.x == 1 && e.y == 0);
    }
}

TEST(Patterns, Zshape) {
    TwoPin tp;
    tp.from.x = 0; tp.from.y = 0; tp.from.z = 0;
    tp.to.x   = 2; tp.to.y   = 2; tp.to.z   = 0;
    Zshape(tp);
    EXPECT_EQ(tp.path.size(), 4u);  // 2 right + 2 up (or symmetric)
}

TEST(Patterns, Monotonic) {
    TwoPin tp;
    tp.from.x = 1; tp.from.y = 0; tp.from.z = 0;
    tp.to.x   = 3; tp.to.y   = 2; tp.to.z   = 0;
    Monotonic(tp);
    EXPECT_EQ(tp.path.size(), 4u);  // dx=2, dy=2
}

TEST(Patterns, ZshapeCostBias) {
    // bias costs so going up first then right is cheaper
    TwoPin tp;
    tp.from.x = 0; tp.from.y = 0; tp.from.z = 0;
    tp.to.x   = 2; tp.to.y   = 2; tp.to.z   = 0;
    auto cost = [](int x, int y, bool hori) {
        (void)x;
        // horizontal edges at y=0 are expensive
        if (hori && y == 0) return 50.0;
        return 1.0;
    };
    Zshape(tp, cost);
    ASSERT_EQ(tp.path.size(), 4u);
    // Expect the first horizontal edge not at y=0
    bool has_hori_y0 = false;
    for (auto& e : tp.path) if (e.hori && e.y == 0) has_hori_y0 = true;
    EXPECT_FALSE(has_hori_y0);
}

TEST(Patterns, MonotonicContinuity) {
    TwoPin tp;
    tp.from.x = 0; tp.from.y = 0; tp.from.z = 0;
    tp.to.x   = 3; tp.to.y   = 3; tp.to.z   = 0;
    Monotonic(tp);
    ASSERT_EQ(tp.path.size(), 6u);  // dx=3, dy=3
    // Paths are traced from target back to source; reconstruct nodes backward.
    std::vector<std::pair<int,int>> nodes;
    int cx = tp.to.x, cy = tp.to.y;
    nodes.emplace_back(cx, cy);
    for (auto& e : tp.path) {
        if (e.hori) cx = e.x;      // horizontal edge (x,y)-(x+1,y) stored at left x
        else        cy = e.y;      // vertical edge (x,y)-(x,y+1) stored at lower y
        nodes.emplace_back(cx, cy);
    }
    // reverse to get from source to target
    std::reverse(nodes.begin(), nodes.end());
    // Continuity: consecutive nodes differ by exactly 1 Manhattan step
    for (size_t i = 1; i < nodes.size(); i++) {
        auto [px, py] = nodes[i-1];
        auto [nx, ny] = nodes[i];
        int md = std::abs(px - nx) + std::abs(py - ny);
        EXPECT_EQ(md, 1);
    }
}


