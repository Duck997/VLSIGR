#include "routing_core.hpp"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <functional>
#include <iostream>

#include "router/patterns.hpp"
#include "router/hum.hpp"
#include "router/utils.hpp"

namespace vlsigr {

namespace {
#ifdef ROUTER_DEBUG
template<typename... Args>
void dbg(Args&&... args) {
    (std::cerr << ... << args) << std::endl;
}
#else
template<typename... Args>
void dbg(Args&&...) {}
#endif
}

void RoutingCore::construct_2D_grid_graph(IspdData& data) {
    // Convert pins to grid coords, dedup, and drop nets that are too big or trivial
    auto keep_end = std::remove_if(data.nets.begin(), data.nets.end(), [&](Net& net) {
        net.pin3D.clear();
        net.pin2D.clear();
        std::vector<std::tuple<int,int,int>> seen3d;
        std::vector<std::pair<int,int>> seen2d;
        for (auto& p : net.pins) {
            int x = (std::get<0>(p) - data.lowerLeftX) / data.tileWidth;
            int y = (std::get<1>(p) - data.lowerLeftY) / data.tileHeight;
            int z = std::get<2>(p) - 1;
            if (std::find(seen3d.begin(), seen3d.end(), std::make_tuple(x,y,z)) == seen3d.end()) {
                seen3d.emplace_back(x,y,z);
                net.pin3D.emplace_back(x,y,z);
            }
            if (std::find(seen2d.begin(), seen2d.end(), std::make_pair(x,y)) == seen2d.end()) {
                seen2d.emplace_back(x,y);
                net.pin2D.emplace_back(x,y,z);
            }
        }
        return net.pin3D.size() > 1000 || net.pin2D.size() <= 1;
    });
    data.nets.erase(keep_end, data.nets.end());
    data.numNet = (int)data.nets.size();

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
}

void RoutingCore::net_decomposition(IspdData& data) {
    for (auto& net : data.nets) {
        auto sz = net.pin2D.size();
        net.twopin.clear();
        if (sz <= 1) continue;
        net.twopin.reserve(sz - 1);
        std::vector<bool> vis(sz, false);
        std::priority_queue<std::tuple<int, size_t, size_t>> pq{};
        auto add = [&](size_t i) {
            vis[i] = true;
            auto [xi, yi, zi] = net.pin2D[i];
            for (size_t j = 0; j < sz; j++) if (!vis[j]) {
                auto [xj, yj, zj] = net.pin2D[j];
                auto d = std::abs(xi - xj) + std::abs(yi - yj);
                pq.emplace(-d, i, j);
            }
        };
        add(0);
        while (!pq.empty()) {
            auto [d, i, j] = pq.top(); pq.pop();
            if (vis[j]) continue;
            TwoPin tp;
            tp.from = net.pin2D[i];
            tp.to   = net.pin2D[j];
            net.twopin.emplace_back(std::move(tp));
            add(j);
        }
        dbg("[decompose] net=", net.name, " twopin=", net.twopin.size());
    }
}

void RoutingCore::preroute(IspdData& data) {
    construct_2D_grid_graph(data);
    net_decomposition(data);
    // Initial pattern routing + place
    for (auto& net : data.nets) {
        for (auto& tp : net.twopin) {
            patterns::Lshape(tp);
            place(tp);
        }
        dbg("[preroute] net=", net.name, " twopin=", net.twopin.size());
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
}

void RoutingCore::del_cost(TwoPin& tp) {
    // Set edge cost to 1 for this twopin path (lock cost)
    for (auto& rp : tp.path) {
        auto& e = grid_.at(rp.x, rp.y, rp.hori);
        e.cost = 1.0;
    }
}

void RoutingCore::del_cost(Net& net) {
    for (auto& tp : net.twopin) {
        for (auto& rp : tp.path) {
            auto& e = grid_.at(rp.x, rp.y, rp.hori);
            e.used += 1;  // protect cost recomputation while net is processed
        }
        del_cost(tp);
    }
}

void RoutingCore::add_cost(TwoPin& tp) {
    for (auto& rp : tp.path) {
        auto& e = grid_.at(rp.x, rp.y, rp.hori);
        if (e.used == 0) {
            e.cost = cost_model_.calc_cost(e);
        }
    }
}

void RoutingCore::add_cost(Net& net) {
    for (auto& tp : net.twopin) {
        for (auto& rp : tp.path) {
            auto& e = grid_.at(rp.x, rp.y, rp.hori);
            e.used -= 1;
        }
        add_cost(tp);
    }
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
    dbg("[check_overflow] tot=", st.tot, " mx=", st.mx, " wl=", st.wl);
    return st;
}

void RoutingCore::ripup_place_once(IspdData& data, const std::function<void(TwoPin&)>& route_func) {
    mark_overflow(data);
    sort_twopins(data);
    for (auto& net : data.nets) {
        del_cost(net);
        for (auto& tp : net.twopin) {
            if (tp.overflow) {
                ripup(tp);
                add_cost(tp);
            }
        }
        for (auto& tp : net.twopin) {
            if (tp.overflow) {
                if (route_func) route_func(tp);
                else route_twopin_default(tp);
                place(tp);
                del_cost(tp);
            }
        }
        add_cost(net);
    }
}

void RoutingCore::route_iterate(IspdData& data, int max_iter,
                                const std::function<void(TwoPin&)>& route_func) {
    int prev_of = std::numeric_limits<int>::max();
    int stall = 0;
    for (int i = 0; i < max_iter; i++) {
        ripup_place_once(data, route_func);
        auto st = check_overflow(data);
        dbg("[iterate] iter=", i+1, " overflow=", st.tot, " mx=", st.mx, " wl=", st.wl);
        if (st.tot == 0) break;
        if (st.tot < prev_of) {
            prev_of = st.tot;
            stall = 0;
        } else {
            stall++;
        }
        if (stall >= 100) break;  // plateau, stop early
    }
}

void RoutingCore::route_pipeline(IspdData& data) {
    // Phase 1: Z-shape (2 iters, selcost=0)
    set_selcost(0);
    dbg("[phase] Zshape start");
    route_iterate(data, 2, [&](TwoPin& tp){ route_twopin_zshape(tp); });
    dbg("[phase] Zshape end");
    // Phase 2: Monotonic (5 iters, selcost=1)
    set_selcost(1);
    dbg("[phase] Monotonic start");
    route_iterate(data, 5, [&](TwoPin& tp){ route_twopin_monotonic(tp); });
    dbg("[phase] Monotonic end");
    // Phase 3: HUM (aggressive, up to 10000 iters with stall early-stop)
    set_selcost(2);
    dbg("[phase] HUM start");
    route_iterate(data, 10000, [&](TwoPin& tp){ route_twopin_hum(tp); });
    dbg("[phase] HUM end");
    // Phase 4: refine WL (selcost=0, monotonic -> Zshape -> Lshape)
    set_selcost(0);
    dbg("[phase] refine WL monotonic start");
    refine_wirelength(data, [&](TwoPin& tp){ route_twopin_monotonic(tp); }, 4);
    dbg("[phase] refine WL monotonic end");
    dbg("[phase] refine WL Zshape start");
    refine_wirelength(data, [&](TwoPin& tp){ route_twopin_zshape(tp); }, 4);
    dbg("[phase] refine WL Zshape end");
    dbg("[phase] refine WL Lshape start");
    refine_wirelength(data, [&](TwoPin& tp){ route_twopin_lshape(tp); }, 4);
    dbg("[phase] refine WL Lshape end");
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

void RoutingCore::refine_wirelength(IspdData& data, const std::function<void(TwoPin&)>& route_func, int iter) {
    check_overflow(data);  // initialize stats
    for (int i = 0; i < iter; ++i) {
        bool improved = false;
        for (auto& net : data.nets) {
            del_cost(net);
            for (auto& tp : net.twopin) {
                if (tp.path.empty()) continue;
                TwoPin cand = tp;
                if (route_func) route_func(cand);
                // only accept if strictly shorter
                if (cand.path.size() >= tp.path.size()) continue;
                auto in_old = [&](const RPoint& rp) {
                    return std::find_if(tp.path.begin(), tp.path.end(), [&](const RPoint& p){
                        return p.x==rp.x && p.y==rp.y && p.hori==rp.hori;
                    }) != tp.path.end();
                };
                bool safe = true;
                for (auto& rp : cand.path) if (!in_old(rp)) {
                    const auto& e = grid_.at(rp.x, rp.y, rp.hori);
                    if (e.demand >= e.cap) { safe = false; break; }
                }
                if (!safe) continue;
                ripup(tp);
                add_cost(tp);
                tp.path = std::move(cand.path);
                place(tp);
                del_cost(tp);
                improved = true;
            }
            add_cost(net);
        }
        auto st = check_overflow(data);
        dbg("[refine_wl] iter=", i+1, " overflow=", st.tot, " mx=", st.mx, " wl=", st.wl);
        if (st.tot > 0) break;
        if (!improved) break;
    }
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



