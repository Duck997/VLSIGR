#include "patterns.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "router/utils.hpp"

namespace vlsigr::patterns {

namespace {

struct Box {
    int L, R, B, U;
    Box(Point f, Point t)
        : L(std::min(f.x, t.x)), R(std::max(f.x, t.x)),
          B(std::min(f.y, t.y)), U(std::max(f.y, t.y)) {}
};

struct BoxCost : Box {
    struct Data {
        double cost = INFINITY;
        std::optional<Point> from = std::nullopt;
    };
    std::vector<Data> cost;
    explicit BoxCost(const Box& box)
        : Box(box), cost((size_t)(R - L + 1) * (size_t)(U - B + 1)) {}
    Data& operator()(int x, int y) {
        auto i = (size_t)(x - L);
        auto j = (size_t)(y - B);
        return cost.at(i * (size_t)(U - B + 1) + j);
    }
    void trace(std::vector<RPoint>& path, Point p) {
        while (true) {
            auto& d = operator()(p.x, p.y);
            if (!d.from.has_value()) break;
            auto prev = d.from.value();
            // emit edge from prev -> p
            auto dx = std::abs(prev.x - p.x);
            auto dy = std::abs(prev.y - p.y);
            if (dx + dy != 1) break;  // safety
            if (dx == 1)
                path.emplace_back(std::min(prev.x, p.x), prev.y, true);
            else
                path.emplace_back(prev.x, std::min(prev.y, p.y), false);
            p = prev;
        }
    }
};

inline double edge_cost(const std::function<double(int,int,bool)>& fn, int x, int y, bool hori) {
    return fn ? fn(x, y, hori) : 1.0;
}

void calcX(BoxCost& box, int y, int bx, int ex, const std::function<double(int,int,bool)>& cost_fn) {
    auto dx = sign(ex - bx);
    if (dx == 0) return;
    double pc = box(bx, y).cost;
    for (auto px = bx, x = px + dx; x != ex + dx; px = x, x += dx) {
        double cc = pc + edge_cost(cost_fn, std::min(x, px), y, true);
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

void calcY(BoxCost& box, int x, int by, int ey, const std::function<double(int,int,bool)>& cost_fn) {
    auto dy = sign(ey - by);
    if (dy == 0) return;
    double pc = box(x, by).cost;
    for (auto py = by, y = py + dy; y != ey + dy; py = y, y += dy) {
        double cc = pc + edge_cost(cost_fn, x, std::min(y, py), false);
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

} // namespace

void Lshape(TwoPin& tp, const std::function<double(int,int,bool)>& cost_fn) {
    auto f = tp.from;
    auto t = tp.to;
    // choose cheaper of (f.x -> t.x then f.y -> t.y) vs the other turn
    Point m1(f.x, t.y, f.z), m2(t.x, f.y, f.z);
    auto eval = [&](Point m) {
        double c = 0;
        auto lineX = [&](int y, int L, int R) {
            if (L > R) std::swap(L, R);
            for (int x = L; x < R; x++) c += edge_cost(cost_fn, x, y, true);
        };
        auto lineY = [&](int x, int B, int U) {
            if (B > U) std::swap(B, U);
            for (int y = B; y < U; y++) c += edge_cost(cost_fn, x, y, false);
        };
        if (f.x != m.x) lineX(f.y, f.x, m.x);
        if (f.y != m.y) lineY(m.x, f.y, m.y);
        if (m.x != t.x) lineX(t.y, m.x, t.x);
        if (m.y != t.y) lineY(m.x, m.y, t.y);
        return c;
    };
    double c1 = eval(m1), c2 = eval(m2);
    Point m = (c1 != c2 ? c1 < c2 : randint<int>(0,1)) ? m1 : m2;
    tp.path.clear();
    auto lineX = [&](int y, int L, int R) {
        if (L > R) std::swap(L, R);
        for (int x = L; x < R; x++) tp.path.emplace_back(x, y, true);
    };
    auto lineY = [&](int x, int B, int U) {
        if (B > U) std::swap(B, U);
        for (int y = B; y < U; y++) tp.path.emplace_back(x, y, false);
    };
    lineX(f.y, f.x, m.x);
    lineY(m.x, f.y, m.y);
    lineX(t.y, m.x, t.x);
    lineY(m.x, m.y, t.y);
}

void Zshape(TwoPin& tp, const std::function<double(int,int,bool)>& cost_fn) {
    auto f = tp.from;
    auto t = tp.to;
    if (f.y > t.y) std::swap(f, t);
    if (f.x > t.x) std::swap(f, t);

    BoxCost boxH(Box(f, t));
    boxH(f.x, f.y).cost = 0.0;
    boxH(f.x, f.y).from = std::nullopt;
    auto boxV = boxH;

    auto dx = sign(t.x - f.x);
    auto dy = sign(t.y - f.y);

    calcX(boxH, f.y, f.x, t.x, cost_fn);
    for (auto px = f.x, x = px + dx; x != t.x + dx; px = x, x += dx)
        calcY(boxH, x, f.y, t.y, cost_fn);
    calcX(boxH, t.y, f.x, t.x, cost_fn);

    calcY(boxV, f.x, f.y, t.y, cost_fn);
    for (auto py = f.y, y = py + dy; y != t.y + dy; py = y, y += dy)
        calcX(boxV, y, f.x, t.x, cost_fn);
    calcY(boxV, t.x, f.y, t.y, cost_fn);

    auto& box = boxV(t.x, t.y).cost < boxH(t.x, t.y).cost ? boxV : boxH;
    tp.path.clear();
    box.trace(tp.path, t);
}

void Monotonic(TwoPin& tp, const std::function<double(int,int,bool)>& cost_fn) {
    auto f = tp.from;
    auto t = tp.to;
    if (f.y > t.y) std::swap(f, t);
    if (f.x > t.x) std::swap(f, t);

    BoxCost box(Box(f, t));
    box(f.x, f.y).cost = 0.0;
    box(f.x, f.y).from = std::nullopt;
    calcX(box, f.y, f.x, t.x, cost_fn);
    calcY(box, f.x, f.y, t.y, cost_fn);
    auto dy = sign(t.y - f.y);
    for (auto py = f.y, y = py + dy; y != t.y + dy; py = y, y += dy) {
        for (auto px = f.x, x = px + 1; x <= t.x; px = x, x++) {
            double cx = box(x, py).cost + edge_cost(cost_fn, x, std::min(y, py), false);
            double cy = box(px, y).cost + edge_cost(cost_fn, std::min(x, px), y, true);
            bool pickX = (cx != cy ? cx < cy : randint<int>(0,1));
            if (pickX) {
                box(x, y).cost = cx;
                box(x, y).from = Point(x, py, 0);
            } else {
                box(x, y).cost = cy;
                box(x, y).from = Point(px, y, 0);
            }
        }
    }
    tp.path.clear();
    box.trace(tp.path, t);
}

}  // namespace vlsigr::patterns



