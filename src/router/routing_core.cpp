#include "routing_core.hpp"

#include <algorithm>
#include <numeric>

#include "router/patterns.hpp"
#include "router/utils.hpp"

namespace vlsigr {

void RoutingCore::preroute(IspdData& data) {
    // Build grid capacities (sum layers for a simple 2D model)
    int vert_cap = std::accumulate(data.verticalCapacity.begin(), data.verticalCapacity.end(), 0);
    int hori_cap = std::accumulate(data.horizontalCapacity.begin(), data.horizontalCapacity.end(), 0);
    grid_.init((size_t)data.numXGrid, (size_t)data.numYGrid, Edge(vert_cap), Edge(hori_cap));
    // Apply capacity adjustments from input (reduce capacity on specific edges)
    for (auto& adj : data.capacityAdjs) {
        auto [x1, y1, z1] = adj.grid1;
        auto [x2, y2, z2] = adj.grid2;
        if (z1 != z2) continue;  // skip cross-layer
        auto z = z1 - 1;
        int lx = std::min(x1, x2), rx = std::max(x1, x2);
        int ly = std::min(y1, y2), ry = std::max(y1, y2);
        int dx = rx - lx, dy = ry - ly;
        if (dx + dy != 1) continue;  // only adjacent grids
        bool hori = dx == 1;
        int layerCap = (hori ? data.horizontalCapacity : data.verticalCapacity)[z];
        int reduce = layerCap - adj.reducedCapacityLevel;
        auto& e = grid_.at(lx, ly, hori);
        e.cap = std::max(0, e.cap - reduce);
    }
    cost_model_.build_cost(grid_);

    // For now, do a trivial decomposition: pair consecutive pins into TwoPin
    for (auto& net : data.nets) {
        net.twopin.clear();
        if (net.pins.size() < 2) continue;
        // Convert to 2D pin list (grid coordinates) and dedup by (x,y)
        net.pin2D.clear();
        std::vector<std::pair<int,int>> seen;
        for (auto& p : net.pins) {
            int x = (std::get<0>(p) - data.lowerLeftX) / data.tileWidth;
            int y = (std::get<1>(p) - data.lowerLeftY) / data.tileHeight;
            int z = std::get<2>(p) - 1;
            if (std::find(seen.begin(), seen.end(), std::make_pair(x, y)) != seen.end()) continue;
            seen.emplace_back(x, y);
            net.pin2D.push_back(Point(x, y, z));
        }
        if (net.pin2D.size() < 2) continue;  // ignore single-pin nets
        // Simple chain: pin i -> pin i+1
        for (size_t i = 0; i + 1 < net.pin2D.size(); ++i) {
            TwoPin tp;
            tp.from = net.pin2D[i];
            tp.to   = net.pin2D[i+1];
            patterns::Lshape(tp);  // unit-cost L-shape
            place(tp);
            net.twopin.push_back(std::move(tp));
        }
    }
}

void RoutingCore::place(TwoPin& tp) {
    for (auto& rp : tp.path) {
        auto& e = grid_.at(rp.x, rp.y, rp.hori);
        e.demand += 1;
    }
}

void RoutingCore::ripup(TwoPin& tp) {
    tp.reroute++;
    for (auto& rp : tp.path) {
        auto& e = grid_.at(rp.x, rp.y, rp.hori);
        e.demand -= 1;
    }
    tp.path.clear();
}

bool RoutingCore::twopin_overflow(const TwoPin& tp) const {
    for (auto& rp : tp.path) {
        const auto& e = grid_.at(rp.x, rp.y, rp.hori);
        if (e.overflow()) return true;
    }
    return false;
}

void RoutingCore::route_twopin(TwoPin& tp) {
    auto cost_fn = [&](int x, int y, bool hori) {
        return cost_model_.calc_cost(grid_.at(x, y, hori));
    };
    patterns::Monotonic(tp, cost_fn);
}

int RoutingCore::check_overflow() const {
    int tot = 0;
    for (auto it = grid_.begin(); it != grid_.end(); ++it) {
        if (it->overflow()) {
            int of = it->demand - it->cap;
            tot += of;
            const_cast<Edge&>(*it).he += of;  // update history
            const_cast<Edge&>(*it).of = 0;
        }
    }
    return tot;
}

void RoutingCore::ripup_place_once(IspdData& data) {
    // Mark overflowed twopins
    for (auto& net : data.nets) {
        for (auto& tp : net.twopin) {
            if (twopin_overflow(tp)) {
                ripup(tp);
                route_twopin(tp);
                place(tp);
            }
        }
    }
    cost_model_.build_cost(grid_);
}

}  // namespace vlsigr



