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
    cost_model_.build_cost(grid_);

    // For now, do a trivial decomposition: pair consecutive pins into TwoPin
    for (auto& net : data.nets) {
        net.twopin.clear();
        if (net.pins.size() < 2) continue;
        // Convert to 2D pin list
        net.pin2D.clear();
        for (auto& p : net.pins) {
            net.pin2D.push_back(Point(std::get<0>(p), std::get<1>(p), std::get<2>(p)-1));
        }
        // Simple chain: pin i -> pin i+1
        for (size_t i = 0; i + 1 < net.pin2D.size(); ++i) {
            TwoPin tp;
            tp.from = net.pin2D[i];
            tp.to   = net.pin2D[i+1];
            patterns::Lshape(tp);  // unit-cost L-shape
            net.twopin.push_back(std::move(tp));
        }
    }
}

}  // namespace vlsigr



