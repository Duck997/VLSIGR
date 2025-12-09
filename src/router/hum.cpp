#include "hum.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "router/patterns.hpp"

namespace vlsigr::hum {

namespace {

struct Box {
    int L, R, B, U;
    bool eL = true, eR = true, eB = true, eU = true;
    Box(Point f, Point t)
        : L(std::min(f.x, t.x)), R(std::max(f.x, t.x)),
          B(std::min(f.y, t.y)), U(std::max(f.y, t.y)) {}
    std::size_t width() const { return (std::size_t)(R - L + 1); }
    std::size_t height() const { return (std::size_t)(U - B + 1); }
};

struct BoxCost : Box {
    struct Data {
        double cost = INFINITY;
        std::optional<Point> from = std::nullopt;
    };
    std::vector<Data> cost;
    explicit BoxCost(const Box& box)
        : Box(box), cost(box.width() * box.height()) {}
    Data& operator()(int x, int y) {
        auto i = (std::size_t)(x - L);
        auto j = (std::size_t)(y - B);
        return cost.at(i * height() + j);
    }
    void trace(std::vector<RPoint>& path, Point p) {
        while (true) {
            auto& d = operator()(p.x, p.y);
            if (!d.from.has_value()) break;
            auto prev = d.from.value();
            auto dx = std::abs(prev.x - p.x);
            auto dy = std::abs(prev.y - p.y);
            if (dx + dy != 1) break;
            if (dx == 1)
                path.emplace_back(std::min(prev.x, p.x), prev.y, true);
            else
                path.emplace_back(prev.x, std::min(prev.y, p.y), false);
            p = prev;
        }
    }
};

inline double edge_cost(CostModel& cm, GridGraph<Edge>& grid, int x, int y, bool hori) {
    return cm.calc_cost(grid.at(x, y, hori));
}

void calcX(BoxCost& box, int y, int bx, int ex,
           CostModel& cm, GridGraph<Edge>& grid) {
    auto dx = (ex > bx) ? 1 : (ex < bx ? -1 : 0);
    if (dx == 0) return;
    double pc = box(bx, y).cost;
    for (auto px = bx, x = px + dx; x != ex + dx; px = x, x += dx) {
        double cc = pc + edge_cost(cm, grid, std::min(x, px), y, true);
        auto& d = box(x, y);
        if (d.cost <= cc) {
            pc = d.cost;
        } else {
            pc = cc;
            d.cost = cc;
            d.from = Point(px, y, 0);
        }
    }
}

void calcY(BoxCost& box, int x, int by, int ey,
           CostModel& cm, GridGraph<Edge>& grid) {
    auto dy = (ey > by) ? 1 : (ey < by ? -1 : 0);
    if (dy == 0) return;
    double pc = box(x, by).cost;
    for (auto py = by, y = py + dy; y != ey + dy; py = y, y += dy) {
        double cc = pc + edge_cost(cm, grid, x, std::min(y, py), false);
        auto& d = box(x, y);
        if (d.cost <= cc) {
            pc = d.cost;
        } else {
            pc = cc;
            d.cost = cc;
            d.from = Point(x, py, 0);
        }
    }
}

}  // namespace

void HUM(TwoPin& tp, GridGraph<Edge>& grid, CostModel& cm, std::size_t width, std::size_t height) {
    bool insert = tp.path.empty();
    static const int BASE_EXPAND = 3;

    if (tp.reroute > 0 || insert) {
        // build or expand box based on overflow direction
        Box box(tp.from, tp.to);
        // heuristic: expand more on axis with overflow counts
        int cntV = 0, cntH = 0;
        for (auto& rp : tp.path) {
            if (grid.at(rp.x, rp.y, rp.hori).overflow())
                rp.hori ? cntH++ : cntV++;
        }
        int d = BASE_EXPAND;
        bool expandH = (cntV != cntH ? cntV > cntH : tp.reroute % 2);  // more V overflow -> expand horizontally
        // if already spanning full width, force vertical expansion
        if (box.width() >= width) expandH = false;
        // if already spanning full height, force horizontal expansion
        if (box.height() >= height) expandH = true;

        if (expandH) {
            if (box.eL) box.L = std::max(0, box.L - d);
            if (box.eR) box.R = std::min((int)width - 1, box.R + d);
        } else {
            if (box.eB) box.B = std::max(0, box.B - d);
            if (box.eU) box.U = std::min((int)height - 1, box.U + d);
        }

        BoxCost CostVF(box), CostHF(box), CostVT(box), CostHT(box);
        auto f = tp.from, t = tp.to;
        CostVF(f.x, f.y).cost = 0.0; CostVF(f.x, f.y).from = std::nullopt;
        CostHF(f.x, f.y).cost = 0.0; CostHF(f.x, f.y).from = std::nullopt;
        CostVT(t.x, t.y).cost = 0.0; CostVT(t.x, t.y).from = std::nullopt;
        CostHT(t.x, t.y).cost = 0.0; CostHT(t.x, t.y).from = std::nullopt;

        // Forward from f
        calcX(CostVF, f.y, box.L, box.R, cm, grid);
        calcX(CostVF, f.y, box.R, box.L, cm, grid);
        for (int y = f.y + 1; y <= box.U; y++) calcX(CostVF, y, box.L, box.R, cm, grid);
        for (int y = f.y - 1; y >= box.B; y--) calcX(CostVF, y, box.L, box.R, cm, grid);
        calcY(CostHF, f.x, box.B, box.U, cm, grid);
        calcY(CostHF, f.x, box.U, box.B, cm, grid);
        for (int x = f.x + 1; x <= box.R; x++) calcY(CostHF, x, box.B, box.U, cm, grid);
        for (int x = f.x - 1; x >= box.L; x--) calcY(CostHF, x, box.B, box.U, cm, grid);

        // From t (reverse)
        calcX(CostVT, t.y, box.L, box.R, cm, grid);
        calcX(CostVT, t.y, box.R, box.L, cm, grid);
        for (int y = t.y + 1; y <= box.U; y++) calcX(CostVT, y, box.L, box.R, cm, grid);
        for (int y = t.y - 1; y >= box.B; y--) calcX(CostVT, y, box.L, box.R, cm, grid);
        calcY(CostHT, t.x, box.B, box.U, cm, grid);
        calcY(CostHT, t.x, box.U, box.B, cm, grid);
        for (int x = t.x + 1; x <= box.R; x++) calcY(CostHT, x, box.B, box.U, cm, grid);
        for (int x = t.x - 1; x >= box.L; x--) calcY(CostHT, x, box.B, box.U, cm, grid);

        auto cF = [&](int x, int y) { return std::min(CostVF(x, y).cost, CostHF(x, y).cost); };
        auto cT = [&](int x, int y) { return std::min(CostVT(x, y).cost, CostHT(x, y).cost); };

        int mx = box.L, my = box.B;
        double mc = cF(mx, my) + cT(mx, my);
        for (int x = box.L; x <= box.R; x++) {
            for (int y = box.B; y <= box.U; y++) {
                double c = cF(x, y) + cT(x, y);
                if (c < mc) { mc = c; mx = x; my = y; }
            }
        }

        tp.path.clear();
        Point meet(mx, my, 0);
        // trace from meet to f and t
        auto trace = [&](BoxCost& CostV, BoxCost& CostH) {
            auto& bc = (CostV(mx, my).cost < CostH(mx, my).cost) ? CostV : CostH;
            bc.trace(tp.path, meet);
        };
        trace(CostVF, CostHF);
        trace(CostVT, CostHT);
    } else {
        // fallback: monotonic
        patterns::Monotonic(tp);
    }
}

}  // namespace vlsigr::hum



