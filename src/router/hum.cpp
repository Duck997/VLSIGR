#include "hum.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>
#include <array>

#include "router/patterns.hpp"
#include "router/utils.hpp"

namespace vlsigr::hum {

namespace {

// Strictly aligned with GlobalRouting::delta (lines 253-262)
inline int delta_from_reroute(int cnt) {
    if (cnt <= 2) return 5;
    if (cnt <= 6) return 20;
    return 15;
}

// Strictly aligned with GlobalRouting::Box (lines 77-85)
struct Box {
    bool eL, eR, eB, eU;
    int L, R, B, U;
    Box(Point f, Point t)
        : eL(true), eR(true), eB(true), eU(true),
          L(std::min(f.x, t.x)), R(std::max(f.x, t.x)),
          B(std::min(f.y, t.y)), U(std::max(f.y, t.y)) {}
    std::size_t width() const { return (std::size_t)(R - L + 1); }
    std::size_t height() const { return (std::size_t)(U - B + 1); }
    Point BL() const { return Point(L, B, 0); }
    Point UR() const { return Point(R, U, 0); }
};

// Strictly aligned with GlobalRouting::BoxCost (lines 87-112)
struct BoxCost : Box {
    struct Data {
        double cost = INFINITY;
        std::optional<Point> from = std::nullopt;
    };
    std::vector<Data> cost;
    explicit BoxCost(const Box& box)
        : Box(box), cost(box.width() * box.height()) {}
    
    Data& operator()(Point p) { return operator()(p.x, p.y); }
    Data& operator()(int x, int y) {
        auto i = (std::size_t)(x - L);
        auto j = (std::size_t)(y - B);
        return cost.at(i * height() + j);
    }
    
    void trace(std::vector<RPoint>& path, Point pp) {
        auto size = path.size() + width() * height();
        while (true) {
            auto ocp = operator()(pp).from;
            if (path.size() > size) break;  // safety guard
            if (!ocp.has_value()) break;
            auto cp = ocp.value();
            // make RPoint from pp to cp
            auto dx = std::abs(pp.x - cp.x);
            auto dy = std::abs(pp.y - cp.y);
            if (dx + dy != 1) break;  // invalid path
            if (dx == 1)
                path.emplace_back(std::min(pp.x, cp.x), pp.y, 0, true);
            else
                path.emplace_back(pp.x, std::min(pp.y, cp.y), 0, false);
            pp = cp;
        }
    }
};

// CRITICAL: Use cached edge.cost, NOT recompute! (aligned with GlobalRouting::cost)
inline double edge_cost(CostModel& /* cm */, GridGraph<Edge>& grid, int x, int y, bool hori) {
    return grid.at(x, y, hori).cost;  // Use cached cost, do NOT calc_cost!
}

// Strictly aligned with GlobalRouting::calcX (lines 396-413)
inline void calcX(BoxCost& box, int y, int bx, int ex,
                  CostModel& cm, GridGraph<Edge>& grid) {
    auto dx = sign(ex - bx);
    if (dx == 0) return;
    auto pc = box(bx, y).cost;
    for (auto px = bx, x = px + dx; x != ex + dx; px = x, x += dx) {
        auto cc = pc + edge_cost(cm, grid, std::min(x, px), y, true);
        auto& data = box(x, y);
        if (data.cost <= cc) {
            pc = data.cost;
        } else {
            pc = cc;
            data.cost = cc;
            data.from = Point(px, y, 0);
        }
    }
}

// Strictly aligned with GlobalRouting::calcY (lines 415-432)
inline void calcY(BoxCost& box, int x, int by, int ey,
                  CostModel& cm, GridGraph<Edge>& grid) {
    auto dy = sign(ey - by);
    if (dy == 0) return;
    auto pc = box(x, by).cost;
    for (auto py = by, y = py + dy; y != ey + dy; py = y, y += dy) {
        auto cc = pc + edge_cost(cm, grid, x, std::min(y, py), false);
        auto& data = box(x, y);
        if (data.cost <= cc) {
            pc = data.cost;
        } else {
            pc = cc;
            data.cost = cc;
            data.from = Point(x, py, 0);
        }
    }
}

// Strictly aligned with GlobalRouting::VMR_impl (lines 470-488)
void VMR_impl(Point f, Point t, BoxCost& box, CostModel& cm, GridGraph<Edge>& grid) {
    box(f).cost = 0;
    box(f).from = std::nullopt;
    calcX(box, f.y, box.L, box.R, cm, grid);
    calcX(box, f.y, box.R, box.L, cm, grid);
    auto dy = sign(t.y - f.y);
    for (auto py = f.y, y = py + dy; y != t.y + dy; py = y, y += dy) {
        for (auto x = box.L; x <= box.R; x++) {
            box(x, y).cost = box(x, py).cost + edge_cost(cm, grid, x, std::min(y, py), false);
            box(x, y).from = Point(x, py, 0);
        }
        calcX(box, y, box.L, box.R, cm, grid);
        calcX(box, y, box.R, box.L, cm, grid);
    }
}

// Strictly aligned with GlobalRouting::HMR_impl (lines 490-508)
void HMR_impl(Point f, Point t, BoxCost& box, CostModel& cm, GridGraph<Edge>& grid) {
    box(f).cost = 0;
    box(f).from = std::nullopt;
    calcY(box, f.x, box.B, box.U, cm, grid);
    calcY(box, f.x, box.U, box.B, cm, grid);
    auto dx = sign(t.x - f.x);
    for (auto px = f.x, x = px + dx; x != t.x + dx; px = x, x += dx) {
        for (auto y = box.B; y <= box.U; y++) {
            box(x, y).cost = box(px, y).cost + edge_cost(cm, grid, std::min(x, px), y, true);
            box(x, y).from = Point(px, y, 0);
        }
        calcY(box, x, box.B, box.U, cm, grid);
        calcY(box, x, box.U, box.B, cm, grid);
    }
}

}  // namespace

// Strictly aligned with GlobalRouting::HUM serial version (lines 510-704)
void HUM(TwoPin& tp, GridGraph<Edge>& grid, CostModel& cm, std::size_t width, std::size_t height) {
    bool insert = false;
    if (tp.box == nullptr) {
        insert = true;
        tp.box = new Box(tp.from, tp.to);
    }
    auto& box = *(Box*)tp.box;

    // Congestion-aware Bounding Box Expansion (lines 519-540)
    if (insert || true) {  // always expand, matching GlobalRouting line 519
        std::array<int, 2> CntOE{0, 0};
        for (auto& rp : tp.path)
            if (grid.at(rp.x, rp.y, rp.hori).overflow())
                CntOE[rp.hori]++;
        
        auto d = delta_from_reroute(tp.reroute);
        auto cV = CntOE[0], cH = CntOE[1];
        
        // line 530: auto lr = cV != cH ? cV > cH : randint(2);
        auto lr = (cV != cH) ? (cV > cH) : (vlsigr::randint(2) != 0);
        
        // lines 532-540
        if ((lr && box.width() != width - 1) || (!lr && box.height() == height - 1)) {
            // horizontal expansion
            if (box.eL) box.L = std::max(0, box.L - d);
            if (box.eR) box.R = std::min((int)width - 1, box.R + d);
        } else {
            // vertical expansion
            if (box.eB) box.B = std::max(0, box.B - d);
            if (box.eU) box.U = std::min((int)height - 1, box.U + d);
        }
    }

    auto f = tp.from, t = tp.to;
    BoxCost CostVF(box), CostHF(box), CostVT(box), CostHT(box);
    
    // Sequential for serial version (lines 589-603)
    if (std::abs(f.x - t.x) == (box.R - box.L)) {
        VMR_impl(f, box.BL(), CostVF, cm, grid); VMR_impl(f, box.UR(), CostVF, cm, grid);
        VMR_impl(t, box.BL(), CostVT, cm, grid); VMR_impl(t, box.UR(), CostVT, cm, grid);
    } else if (std::abs(f.y - t.y) == (box.U - box.B)) {
        HMR_impl(f, box.BL(), CostHF, cm, grid); HMR_impl(f, box.UR(), CostHF, cm, grid);
        HMR_impl(t, box.BL(), CostHT, cm, grid); HMR_impl(t, box.UR(), CostHT, cm, grid);
    } else {
        VMR_impl(f, box.BL(), CostVF, cm, grid); VMR_impl(f, box.UR(), CostVF, cm, grid);
        HMR_impl(f, box.BL(), CostHF, cm, grid); HMR_impl(f, box.UR(), CostHF, cm, grid);
        VMR_impl(t, box.BL(), CostVT, cm, grid); VMR_impl(t, box.UR(), CostVT, cm, grid);
        HMR_impl(t, box.BL(), CostHT, cm, grid); HMR_impl(t, box.UR(), CostHT, cm, grid);
    }
    
    // lines 604-612
    auto cF = [&](int x, int y) {
        return std::min(CostVF(x, y).cost, CostHF(x, y).cost);
    };
    auto cT = [&](int x, int y) {
        return std::min(CostVT(x, y).cost, CostHT(x, y).cost);
    };
    auto calc = [&](int x, int y) {
        return cF(x, y) + cT(x, y);
    };
    
    // lines 651-662: sequential minimum search
    auto mx = box.L, my = box.B;
    auto mc = calc(mx, my);
    for (auto y = box.B; y <= box.U; y++) {
        for (auto x = box.L; x <= box.R; x++) {
            auto c = calc(x, y);
            if (c < mc) {
                mx = x;
                my = y;
                mc = c;
            }
        }
    }
    
    // lines 663-670
    tp.path.clear();
    Point m(mx, my, 0);
    auto trace = [&](BoxCost& CostV, BoxCost& CostH) {
        auto& cost = (CostV(mx, my).cost < CostH(mx, my).cost) ? CostV : CostH;
        cost.trace(tp.path, m);
    };
    trace(CostVF, CostHF);
    trace(CostVT, CostHT);

    // lines 672-703: boundary update
    constexpr double alpha = 1;
    auto update = [&](int L, int R, int B, int U) {
        auto ec = calc(L, B);
        for (int ux = L; ux <= R; ux++) 
            for (int uy = B; uy <= U; uy++)
                for (int vx = L; vx <= R; vx++) 
                    for (int vy = B; vy <= U; vy++) {
                        auto d = std::abs(ux - vx) + std::abs(uy - vy);
                        auto c = cF(ux, uy) + cT(vx, vy) + d * alpha;
                        if (c < ec) ec = c;
                    }
        return mc >= ec;
    };
    // lines 698-702
    box.eL = update(box.L, box.L, box.B, box.U);
    box.eR = update(box.R, box.R, box.B, box.U);
    box.eB = update(box.L, box.R, box.B, box.B);
    box.eU = update(box.L, box.R, box.U, box.U);
}

}  // namespace vlsigr::hum
