#pragma once

#include <functional>
#include <vector>

#include "router/ispd_data.hpp"
#include "router/grid_graph.hpp"
#include "router/cost_model.hpp"

namespace vlsigr {

// Forward declaration
using TwoPinPtr = TwoPin*;

class RoutingCore {
public:
    // Net wrapper struct (strictly aligned with GlobalRouting::Net, lines 62-68)
    struct NetWrapper {
        int overflow, overflow_twopin, wlen, reroute;
        double score, cost;
        Net* net;  // pointer to original net in IspdData
        std::vector<TwoPinPtr> twopins;
        explicit NetWrapper(Net* n);
    };

    struct OverflowStats {
        int tot = 0;
        int mx = 0;
        int wl = 0;
    };

    RoutingCore();
    ~RoutingCore();

    // Main routing entry (aligned with GlobalRouting::route, line 865)
    void route(IspdData& data, bool leave = false);
    
    // Core routing functions
    void preroute(IspdData& data);
    void route_pipeline(IspdData& data);
    
    const GridGraph<Edge>& grid() const { return grid_; }
    
private:
    // Core data structures (aligned with GlobalRouting, lines 80-84)
    std::size_t width_, height_;
    int min_width_, min_spacing_, min_net_, mx_cap_;
    GridGraph<Edge> grid_;
    std::vector<NetWrapper*> nets_;
    std::vector<TwoPinPtr> twopins_;
    
    int selcost_;
    CostModel cost_model_;
    bool stop_, print_;
    IspdData* ispdData_;

    // Core routing operations (aligned with GlobalRouting)
    void ripup(TwoPinPtr twopin);
    void place(TwoPinPtr twopin);
    
    void del_cost(NetWrapper* net);
    void del_cost(TwoPinPtr twopin);
    void add_cost(NetWrapper* net);
    void add_cost(TwoPinPtr twopin);
    
    int check_overflow();
    void sort_twopins();
    
    inline double score(const TwoPinPtr twopin) const;
    inline double score(const NetWrapper* net) const;
    inline int delta(const TwoPinPtr twopin) const;
    
    inline double cost(const TwoPinPtr twopin) const;
    inline double cost(Point f, Point t) const;
    inline double cost(RPoint rp) const;
    inline double cost(int x, int y, bool hori) const;
    inline double cost(const Edge& e) const;
    
    // Routing algorithms (function pointer type)
    using FP = void (RoutingCore::*)(TwoPinPtr);
    void Lshape(TwoPinPtr twopin);
    void Zshape(TwoPinPtr twopin);
    void monotonic(TwoPinPtr twopin);
    void HUM(TwoPinPtr twopin);
    
    // Routing phases
    void routing(const char* name, FP fp, int iteration, int sel_cost);
    void ripup_place(FP fp);
    void refine_wirelength(const char* name, FP fp, int iteration, int sel_cost);
    void ripup_place_wl(FP fp);
    
    // Grid construction
    void construct_2D_grid_graph();
    void net_decomposition();
    
    // Helper for cost calculation
    void build_cost();
    
    inline const Edge& getEdge(RPoint rp) const { return grid_.at(rp.x, rp.y, rp.hori); }
    inline Edge& getEdge(RPoint rp) { return grid_.at(rp.x, rp.y, rp.hori); }
    inline const Edge& getEdge(int x, int y, bool hori) const { return grid_.at(x, y, hori); }
    inline Edge& getEdge(int x, int y, bool hori) { return grid_.at(x, y, hori); }
};

}  // namespace vlsigr
