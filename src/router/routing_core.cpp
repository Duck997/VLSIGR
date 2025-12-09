#include "routing_core.hpp"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <functional>

#include "router/patterns.hpp"
#include "router/hum.hpp"
#include "router/utils.hpp"

namespace vlsigr {

void RoutingCore::preroute(IspdData& data) {
    // Build grid capacities (sum layers for a simple 2D model), scaled by min_net like legacy
    int min_w = average(data.minimumWidth);
    int min_s = average(data.minimumSpacing);
    int min_net = std::max(1, min_w + min_s);
    int vert_cap = std::accumulate(data.verticalCapacity.begin(), data.verticalCapacity.end(), 0) / min_net;
    int hori_cap = std::accumulate(data.horizontalCapacity.begin(), data.horizontalCapacity.end(), 0) / min_net;
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
        int reduce = (layerCap - adj.reducedCapacityLevel) / min_net;
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

void RoutingCore::mark_overflow(IspdData& data) {
    // reset stats
    for (auto& net : data.nets) {
        net.overflow = 0;
        net.overflow_twopin = 0;
        net.wlen = 0;
        net.cost = 0.0;
        for (auto& tp : net.twopin) tp.overflow = false;
    }

    // temp used accumulation like legacy
    for (auto& net : data.nets) {
        for (auto& tp : net.twopin) {
            for (auto& rp : tp.path) {
                auto& e = grid_.at(rp.x, rp.y, rp.hori);
                e.used += 1;
                if (e.used == 1) net.wlen++;
                if (e.overflow()) {
                    tp.overflow = true;
                    if (e.used == 1) {
                        net.cost += cost_model_.calc_cost(e);
                        net.overflow++;
                    }
                }
            }
            if (tp.overflow) net.overflow_twopin++;
        }
    }
    // rollback used
    for (auto& net : data.nets)
        for (auto& tp : net.twopin)
            for (auto& rp : tp.path)
                grid_.at(rp.x, rp.y, rp.hori).used -= 1;
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

void RoutingCore::route_twopin_monotonic(TwoPin& tp) {
    auto cost_fn = [&](int x, int y, bool hori) {
        return cost_model_.calc_cost(grid_.at(x, y, hori));
    };
    patterns::Monotonic(tp, cost_fn);
}

void RoutingCore::route_twopin_lshape(TwoPin& tp) {
    patterns::Lshape(tp);
}

void RoutingCore::route_twopin_zshape(TwoPin& tp) {
    auto cost_fn = [&](int x, int y, bool hori) {
        return cost_model_.calc_cost(grid_.at(x, y, hori));
    };
    patterns::Zshape(tp, cost_fn);
}

void RoutingCore::route_twopin_hum(TwoPin& tp) {
    hum::HUM(tp, grid_, cost_model_, grid_.width(), grid_.height());
}

void RoutingCore::route_twopin_default(TwoPin& tp) {
    if (tp.overflow || selcost_ == 2) {
        route_twopin_hum(tp);
    } else {
        route_twopin_monotonic(tp);
    }
}

RoutingCore::OverflowStats RoutingCore::check_overflow(IspdData& data) {
    OverflowStats st;
    // compute edge overflow stats and update history
    for (auto it = grid_.begin(); it != grid_.end(); ++it) {
        if (it->overflow()) {
            int of = it->demand - it->cap;
            st.tot += of;
            st.mx = std::max(st.mx, of);
            const_cast<Edge&>(*it).he += of;  // update history
            const_cast<Edge&>(*it).of = 0;
        }
    }
    // wirelength: count unique edges used
    int wl = 0;
    for (auto& net : data.nets) {
        for (auto& tp : net.twopin) {
            for (auto& rp : tp.path) {
                auto& e = grid_.at(rp.x, rp.y, rp.hori);
                bool first = (e.used++ == 0);
                if (first) wl++;
            }
        }
    }
    // also propagate overflow flags to twopins/nets for sorting in next iteration
    for (auto& net : data.nets) {
        net.overflow = 0;
        net.overflow_twopin = 0;
        for (auto& tp : net.twopin) {
            tp.overflow = false;
            for (auto& rp : tp.path) {
                const auto& e = grid_.at(rp.x, rp.y, rp.hori);
                if (e.overflow()) {
                    tp.overflow = true;
                    break;
                }
            }
            if (tp.overflow) net.overflow_twopin++;
        }
    }

    for (auto& net : data.nets)
        for (auto& tp : net.twopin)
            for (auto& rp : tp.path)
                grid_.at(rp.x, rp.y, rp.hori).used--;
    st.wl = wl;
    return st;
}

void RoutingCore::ripup_place_once(IspdData& data, const std::function<void(TwoPin&)>& route_func) {
    mark_overflow(data);
    sort_twopins(data);
    // Mark overflowed twopins
    for (auto& net : data.nets) {
        for (auto& tp : net.twopin) {
            if (twopin_overflow(tp)) {
                ripup(tp);
                if (route_func) route_func(tp);
                else route_twopin_default(tp);
                place(tp);
            }
        }
    }
    cost_model_.build_cost(grid_);
}

void RoutingCore::route_iterate(IspdData& data, int max_iter,
                                const std::function<void(TwoPin&)>& route_func) {
    int prev_of = std::numeric_limits<int>::max();
    for (int i = 0; i < max_iter; i++) {
        ripup_place_once(data, route_func);
        auto st = check_overflow(data);
        if (st.tot == 0) break;
        if (st.tot >= prev_of) break;  // no improvement
        prev_of = st.tot;
    }
}

void RoutingCore::route_pipeline(IspdData& data) {
    // Phase 1: Z-shape refinement (light selcost)
    set_selcost(0);
    route_iterate(data, 3, [&](TwoPin& tp){ route_twopin_zshape(tp); });
    // Phase 2: Monotonic with higher selcost
    set_selcost(1);
    route_iterate(data, 5, [&](TwoPin& tp){ route_twopin_monotonic(tp); });
    // Phase 3: HUM aggressive
    set_selcost(2);
    route_iterate(data, 10, [&](TwoPin& tp){ route_twopin_hum(tp); });
}

int RoutingCore::hpwl(const TwoPin& tp) const {
    return std::abs(tp.from.x - tp.to.x) + std::abs(tp.from.y - tp.to.y);
}

double RoutingCore::score_twopin(const TwoPin& tp) const {
    int dx = 1 + std::abs(tp.from.x - tp.to.x);
    int dy = 1 + std::abs(tp.from.y - tp.to.y);
    if (selcost_ == 2) {
        return 60.0 * (tp.overflow ? 1.0 : 0.0) + 1.0 * (int)tp.path.size();
    }
    if (selcost_ == 1) {
        return 60.0 * (tp.overflow ? 1.0 : 0.0) + (dx * dy);
    }
    return 100.0 / std::max(dx, dy);
}

double RoutingCore::score_net(const Net& net) const {
    double cost = net.cost <= 0 ? 1.0 : net.cost;
    return 10.0 * net.overflow + net.overflow_twopin + 3.0 * std::log2(cost);
}

void RoutingCore::sort_twopins(IspdData& data) {
    // sort nets by descending score
    std::sort(data.nets.begin(), data.nets.end(), [&](const Net& a, const Net& b) {
        return score_net(a) > score_net(b);
    });
    // sort twopins inside each net (ascending score, then smaller HPWL)
    for (auto& net : data.nets) {
        std::sort(net.twopin.begin(), net.twopin.end(), [&](const TwoPin& a, const TwoPin& b) {
            double sa = score_twopin(a);
            double sb = score_twopin(b);
            if (sa != sb) return sa < sb;
            return hpwl(a) < hpwl(b);
        });
    }
}

}  // namespace vlsigr



