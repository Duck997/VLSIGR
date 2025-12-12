#include "routing_core.hpp"

#include <algorithm>
#include <numeric>
#include <queue>
#include <cmath>
#include <limits>
#include <iostream>
#include <chrono>

#include "router/patterns.hpp"
#include "router/hum.hpp"
#include "router/utils.hpp"

// Debug macro (enabled by -DROUTER_DEBUG)
#ifdef ROUTER_DEBUG
#define dbg(...) std::cerr << __VA_ARGS__ << std::endl
#else
#define dbg(...) do {} while(0)
#endif

namespace vlsigr {

// NetWrapper constructor
RoutingCore::NetWrapper::NetWrapper(Net* n)
    : overflow(0), overflow_twopin(0), wlen(0), reroute(0),
      score(0), cost(0), net(n), twopins{} {}

// Constructor
RoutingCore::RoutingCore()
    : width_(0), height_(0), min_width_(0), min_spacing_(0), min_net_(0), mx_cap_(0),
      selcost_(0), stop_(false), print_(true), ispdData_(nullptr), cfg_{} {}

// Destructor
RoutingCore::~RoutingCore() {
    for (auto net : nets_) delete net;
}

// ripup
void RoutingCore::ripup(TwoPinPtr twopin) {
    if (twopin->ripup) return;
    twopin->ripup = true;
    twopin->reroute++;
    for (auto rp : twopin->path) {
        auto& e = getEdge(rp);
        bool zero = (e.used == 1);
        if (zero) e.demand--;
        e.used--;
    }
}

// place
void RoutingCore::place(TwoPinPtr twopin) {
    if (!twopin->ripup) {
        std::cerr << "[WARNING] place called on non-ripup twopin!" << std::endl;
        return;
    }
    twopin->ripup = false;
    for (auto rp : twopin->path) {
        auto& e = getEdge(rp);
        if (twopin->overflow) e.of++;
        bool zero = (e.used == 0);
        if (zero) e.demand++;
        e.used++;
    }
}

// cost inline functions
inline double RoutingCore::cost(const TwoPinPtr twopin) const {
    double c = 0;
    for (auto rp : twopin->path)
        c += cost(rp);
    return c;
}

inline double RoutingCore::cost(Point f, Point t) const {
    auto dx = std::abs(f.x - t.x);
    auto dy = std::abs(f.y - t.y);
    if (dx == 1 && dy == 0)
        return cost(std::min(f.x, t.x), f.y, true);
    if (dx == 0 && dy == 1)
        return cost(f.x, std::min(f.y, t.y), false);
    return INFINITY;
}

inline double RoutingCore::cost(RPoint rp) const {
    return cost(getEdge(rp));
}

inline double RoutingCore::cost(int x, int y, bool hori) const {
    return cost(getEdge(x, y, hori));
}

inline double RoutingCore::cost(const Edge& e) const {
    return e.cost;
}

// del_cost for net
void RoutingCore::del_cost(NetWrapper* net) {
    for (auto twopin : net->twopins)
        for (auto rp : twopin->path)
            getEdge(rp).used++;
    for (auto twopin : net->twopins)
        del_cost(twopin);
}

// del_cost for twopin
void RoutingCore::del_cost(TwoPinPtr twopin) {
    for (auto rp : twopin->path)
        getEdge(rp).cost = 1;
}

// add_cost for net
void RoutingCore::add_cost(NetWrapper* net) {
    for (auto twopin : net->twopins)
        for (auto rp : twopin->path)
            getEdge(rp).used--;
    for (auto twopin : net->twopins)
        add_cost(twopin);
}

// add_cost for twopin
void RoutingCore::add_cost(TwoPinPtr twopin) {
    for (auto rp : twopin->path) {
        auto& e = getEdge(rp);
        if (e.used == 0)
            e.cost = cost_model_.calc_cost(e);
    }
}

// build_cost
void RoutingCore::build_cost() {
    cost_model_.build_cost(grid_);
}

// sort_twopins
void RoutingCore::sort_twopins() {
    std::sort(nets_.begin(), nets_.end(), [&](auto a, auto b) {
        auto sa = score(a);
        auto sb = score(b);
        return sa > sb;
    });
    for (auto net : nets_)
        std::sort(net->twopins.begin(), net->twopins.end(), [&](auto a, auto b) {
            auto sa = score(a);
            auto sb = score(b);
            auto hpwl_a = std::abs(a->from.x - a->to.x) + std::abs(a->from.y - a->to.y);
            auto hpwl_b = std::abs(b->from.x - b->to.x) + std::abs(b->from.y - b->to.y);
            return sa != sb ? sa < sb : hpwl_a < hpwl_b;
        });
}

// score for twopin
inline double RoutingCore::score(const TwoPinPtr twopin) const {
    if (selcost_ == 2)
        return 60 * (twopin->overflow ? 1 : 0) + 1 * (int)twopin->path.size();
    
    auto dx = 1 + std::abs(twopin->from.x - twopin->to.x);
    auto dy = 1 + std::abs(twopin->from.y - twopin->to.y);
    
    if (selcost_ == 1)
        return 60 * (twopin->overflow ? 1 : 0) + (dx * dy);
    
    return 100.0 / std::max(dx, dy);
}

// score for net
inline double RoutingCore::score(const NetWrapper* net) const {
    return 10 * net->overflow + net->overflow_twopin + 3 * std::log2(net->cost);
}

// delta
inline int RoutingCore::delta(const TwoPinPtr twopin) const {
    int cnt = twopin->reroute;
    if (cnt <= 2) return 5;
    if (cnt <= 6) return 20;
    return 15;
}

// check_overflow
int RoutingCore::check_overflow() {
    int mxof = 0, totof = 0;
    
    for (auto& edge : grid_) {
        edge.he += edge.of;
        edge.of = 0;
        if (edge.overflow()) {
            auto of = edge.demand - edge.cap;
            totof += of;
            if (of > mxof) mxof = of;
        }
    }
    
    int ofnet = 0, oftp = 0, wl = 0;
    
    for (auto net : nets_) {
        net->cost = net->wlen = net->overflow = net->overflow_twopin = 0;
        for (auto twopin : net->twopins) {
            twopin->overflow = false;
            for (auto rp : twopin->path) {
                auto& e = getEdge(rp);
                bool zero = (e.used++ == 0);
                if (zero) net->wlen++;
                if (e.overflow()) {
                    twopin->overflow = true;
                    if (zero) {
                        net->cost += cost(e);
                        net->overflow++;
                    }
                }
            }
            if (twopin->overflow) {
                net->overflow_twopin++;
                oftp++;
            }
        }
        wl += net->wlen;
        if (net->overflow)
            ofnet++;
        for (auto twopin : net->twopins)
            for (auto rp : twopin->path)
                getEdge(rp).used--;
    }
    
    if (print_)
        std::cerr << " tot overflow " << totof
                  << " mx overflow " << mxof
                  << " wirelength " << wl
                  << " of net " << ofnet
                  << " of twopin " << oftp << std::endl;
    
    return totof;
}

// Lshape
void RoutingCore::Lshape(TwoPinPtr twopin) {
    patterns::Lshape(*twopin, [&](int x, int y, bool hori) -> double {
        return cost(x, y, hori);
    });
}

// Zshape
void RoutingCore::Zshape(TwoPinPtr twopin) {
    patterns::Zshape(*twopin, [&](int x, int y, bool hori) -> double {
        return cost(x, y, hori);
    });
}

// monotonic
void RoutingCore::monotonic(TwoPinPtr twopin) {
    patterns::Monotonic(*twopin, [&](int x, int y, bool hori) -> double {
        return cost(x, y, hori);
    });
}

// HUM
void RoutingCore::HUM(TwoPinPtr twopin) {
    hum::HUM(*twopin, grid_, cost_model_, width_, height_);
}

// ripup_place
void RoutingCore::ripup_place(FP fp) {
    sort_twopins();
    for (auto net : nets_) {
        for (auto twopin : net->twopins) {
            twopin->overflow = false;
            for (auto rp : twopin->path)
                if (getEdge(rp).overflow()) {
                    twopin->overflow = true;
                    break;
                }
        }
        
        del_cost(net);
        
        for (auto twopin : net->twopins) {
            if (twopin->overflow) {
                ripup(twopin);
                add_cost(twopin);
            }
        }
        
        for (auto twopin : net->twopins) {
            if (twopin->ripup) {
                (this->*fp)(twopin);
                place(twopin);
                del_cost(twopin);
            }
        }
        
        add_cost(net);
    }
    if (stop_) throw false;
}

// ripup_place_wl
void RoutingCore::ripup_place_wl(FP fp) {
    sort_twopins();
    for (auto net : nets_) {
        del_cost(net);
        
        for (auto twopin : net->twopins) {
            if (twopin->path.empty()) continue;
            if (twopin->path.size() <= 2 && 
                std::abs(twopin->from.x - twopin->to.x) + std::abs(twopin->from.y - twopin->to.y) <= 2)
                continue;
            
            auto old_path = twopin->path;
            (this->*fp)(twopin);
            auto candidate = twopin->path;
            twopin->path = old_path;
            
            if (candidate.size() >= old_path.size()) continue;
            
            auto in_old = [&](const RPoint& rp) {
                for (const auto& p : old_path)
                    if (p.x == rp.x && p.y == rp.y && p.z == rp.z && p.hori == rp.hori)
                        return true;
                return false;
            };
            
            bool safe = true;
            for (const auto& rp : candidate) if (!in_old(rp)) {
                const auto& e = getEdge(rp);
                if (e.demand >= e.cap) {
                    safe = false;
                    break;
                }
            }
            if (!safe) continue;
            
            ripup(twopin);
            add_cost(twopin);
            twopin->path = candidate;
            place(twopin);
            del_cost(twopin);
        }
        
        add_cost(net);
    }
    if (stop_) throw false;
}

// routing
void RoutingCore::routing(const char* name, FP fp, int iteration, int sel_cost) {
    selcost_ = sel_cost;
    cost_model_.set_selcost(sel_cost);
    if (print_) std::cerr << "[*] " << name << " routing" << std::endl;
    auto start = std::chrono::steady_clock::now();
    build_cost();
    
    int prev_of = std::numeric_limits<int>::max();
    int stall = 0;
    for (int i = 1; i <= iteration; i++) {
        ripup_place(fp);
        if (print_) std::cerr << " " << i << " time " << sec_since(start) << "s";
        int of = check_overflow();
        if (of == 0) throw true;
        
        if (of < prev_of) {
            prev_of = of;
            stall = 0;
        } else {
            stall++;
        }
        if (stall >= 100) break;
        
        if (stop_) throw false;
    }
    if (print_) std::cerr << name << " routing costs " << sec_since(start) << "s" << std::endl;
}

// refine_wirelength
void RoutingCore::refine_wirelength(const char* name, FP fp, int iteration, int sel_cost) {
    selcost_ = sel_cost;
    cost_model_.set_selcost(sel_cost);
    if (print_) std::cerr << "[*] " << name << " refine WL" << std::endl;
    auto start = std::chrono::steady_clock::now();
    build_cost();
    
    for (int i = 1; i <= iteration; i++) {
        ripup_place_wl(fp);
        if (print_) std::cerr << " " << i << " time " << sec_since(start) << "s";
        int of = check_overflow();
        if (of > 0) {
            if (print_) std::cerr << " refine aborted due to OF>0 " << of << std::endl;
            break;
        }
        if (stop_) throw false;
    }
    if (print_) std::cerr << name << " refine WL costs " << sec_since(start) << "s" << std::endl;
}

// preroute
void RoutingCore::preroute(IspdData& /* data */) {
    if (print_) std::cerr << "[*] preroute" << std::endl;
    auto start = std::chrono::steady_clock::now();
    
    for (auto net : nets_) {
        net->wlen = 0;
        for (auto twopin : net->twopins) {
            auto hpwl = std::abs(twopin->from.x - twopin->to.x) + std::abs(twopin->from.y - twopin->to.y);
            net->wlen += hpwl;
        }
    }
    
    sort_twopins();
    build_cost();
    
    for (auto net : nets_) {
        net->wlen = 0;
        for (auto twopin : net->twopins) {
            twopin->ripup = true;
            Lshape(twopin);
            place(twopin);
            del_cost(twopin);
        }
        add_cost(net);
    }
    
    if (print_) std::cerr << " time " << sec_since(start) << "s";
    check_overflow();
}

// construct_2D_grid_graph
void RoutingCore::construct_2D_grid_graph() {
    // Filter nets: remove nets with >1000 pins or <=1 2D pins
    ispdData_->nets.erase(
        std::remove_if(ispdData_->nets.begin(), ispdData_->nets.end(), [&](auto& net) {
            for (auto& _pin : net.pins) {
                int x = (std::get<0>(_pin) - ispdData_->lowerLeftX) / ispdData_->tileWidth;
                int y = (std::get<1>(_pin) - ispdData_->lowerLeftY) / ispdData_->tileHeight;
                int z = std::get<2>(_pin) - 1;
                
                if (std::any_of(net.pin3D.begin(), net.pin3D.end(), [x, y, z](const auto& pin) {
                    return pin.x == x && pin.y == y && pin.z == z;
                })) continue;
                net.pin3D.emplace_back(x, y, z);
                
                if (std::any_of(net.pin2D.begin(), net.pin2D.end(), [x, y](const auto& pin) {
                    return pin.x == x && pin.y == y;
                })) continue;
                net.pin2D.emplace_back(x, y, 0);
            }
            
            return net.pin3D.size() > 1000 || net.pin2D.size() <= 1;
        }),
        ispdData_->nets.end()
    );
    ispdData_->numNet = (int)ispdData_->nets.size();
    
    auto verticalCapacity = std::accumulate(ispdData_->verticalCapacity.begin(),
                                            ispdData_->verticalCapacity.end(), 0);
    auto horizontalCapacity = std::accumulate(ispdData_->horizontalCapacity.begin(),
                                              ispdData_->horizontalCapacity.end(), 0);
    verticalCapacity /= min_net_;
    horizontalCapacity /= min_net_;
    mx_cap_ = std::max(verticalCapacity, horizontalCapacity);
    
    grid_.init(width_, height_, Edge(verticalCapacity), Edge(horizontalCapacity));
    
    for (auto& capacityAdj : ispdData_->capacityAdjs) {
        auto [x1, y1, z1] = capacityAdj.grid1;
        auto [x2, y2, z2] = capacityAdj.grid2;
        if (z1 != z2) continue;
        auto z = (std::size_t)z1 - 1;
        auto lx = std::min(x1, x2), rx = std::max(x1, x2);
        auto ly = std::min(y1, y2), ry = std::max(y1, y2);
        auto dx = rx - lx, dy = ry - ly;
        if (dx + dy != 1) continue;
        auto hori = (dx != 0);
        auto& edge = getEdge(lx, ly, hori);
        auto layerCap = hori ? ispdData_->horizontalCapacity[z] : ispdData_->verticalCapacity[z];
        edge.cap -= (layerCap - capacityAdj.reducedCapacityLevel) / min_net_;
    }
}

// net_decomposition
void RoutingCore::net_decomposition() {
    for (auto& net : ispdData_->nets) {
        auto sz = net.pin2D.size();
        net.twopin.clear();
        net.twopin.reserve(sz - 1);
        std::vector<bool> vis(sz, false);
        std::priority_queue<std::tuple<int, std::size_t, std::size_t>> pq{};
        
        auto add = [&](std::size_t i) {
            vis[i] = true;
            auto [xi, yi, zi] = net.pin2D[i];
            for (std::size_t j = 0; j < sz; j++) if (!vis[j]) {
                auto [xj, yj, zj] = net.pin2D[j];
                auto d = std::abs(xi - xj) + std::abs(yi - yj);
                pq.emplace(-d, i, j);
            }
        };
        
        add(0);
        while (!pq.empty()) {
            auto [d, i, j] = pq.top();
            pq.pop();
            if (vis[j]) continue;
            TwoPin tp;
            tp.from = net.pin2D[i];
            tp.to = net.pin2D[j];
            net.twopin.emplace_back(tp);
            add(j);
        }
    }
}

// route
void RoutingCore::route(IspdData& data, bool leave) {
    ispdData_ = &data;
    width_ = (std::size_t)ispdData_->numXGrid;
    height_ = (std::size_t)ispdData_->numYGrid;
    min_width_ = average(ispdData_->minimumWidth);
    min_spacing_ = average(ispdData_->minimumSpacing);
    min_net_ = min_width_ + min_spacing_;
    
    construct_2D_grid_graph();
    net_decomposition();
    
    // Build Net wrappers
    for (auto net : nets_) delete net;
    nets_.clear();
    nets_.reserve(ispdData_->nets.size());
    auto twopin_count = std::accumulate(ispdData_->nets.begin(), ispdData_->nets.end(), 0u,
                                        [&](auto s, auto& net) {
                                            return s + net.twopin.size();
                                        });
    twopins_.reserve(twopin_count);
    
    for (auto& net : ispdData_->nets) {
        auto mynet = new NetWrapper(&net);
        mynet->twopins.reserve(net.twopin.size());
        nets_.emplace_back(mynet);
        for (auto& twopin : net.twopin) {
            twopins_.emplace_back(&twopin);
            mynet->twopins.emplace_back(&twopin);
        }
    }
    
    // Select initial selcost
    if (cfg_.adaptive_scoring) {
        selcost_ = cfg_.selcost_pattern;
    } else {
        selcost_ = cfg_.selcost_fixed;
    }
    cost_model_.set_selcost(selcost_);
    preroute(data);
    if (leave) return;
    
    auto sel_for = [&](int phase_selcost) -> int {
        return cfg_.adaptive_scoring ? phase_selcost : cfg_.selcost_fixed;
    };

    try {
        if (cfg_.iter_lshape > 0)
            routing("Lshape", &RoutingCore::Lshape, cfg_.iter_lshape, sel_for(cfg_.selcost_pattern));
    } catch (bool done) { if (!done) throw; }

    try {
        if (cfg_.iter_zshape > 0)
            routing("Zshape", &RoutingCore::Zshape, cfg_.iter_zshape, sel_for(cfg_.selcost_pattern));
    } catch (bool done) { if (!done) throw; }

    try {
        if (cfg_.iter_monotonic > 0)
            routing("monotonic", &RoutingCore::monotonic, cfg_.iter_monotonic, sel_for(cfg_.selcost_monotonic));
    } catch (bool done) { if (!done) throw; }

    if (cfg_.enable_hum) {
        try {
            if (cfg_.iter_hum > 0)
                routing("HUM", &RoutingCore::HUM, cfg_.iter_hum, sel_for(cfg_.selcost_hum));
        } catch (bool done) { if (!done) throw; }
    }

    if (cfg_.enable_refine) {
        const int it = cfg_.refine_iters;
        const int sel = sel_for(cfg_.selcost_refine);
        refine_wirelength("refine WL monotonic", &RoutingCore::monotonic, it, sel);
        refine_wirelength("refine WL Zshape", &RoutingCore::Zshape, it, sel);
        refine_wirelength("refine WL Lshape", &RoutingCore::Lshape, it, sel);
    }
}

// Legacy interface for backward compatibility
void RoutingCore::route_pipeline(IspdData& data) {
    route(data, false);
}

}  // namespace vlsigr
