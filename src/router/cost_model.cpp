#include "cost_model.hpp"

namespace vlsigr {

void CostModel::build_cost_pe() {
    constexpr double z = 200.0;
    for (int i = 0; i < COSTSZ; i++) {
        int of = i - COSTOFF;
        switch (selcost) {
            case 0:
                cost_pe[i] = 1 + z / (1 + std::exp(-0.3 * of));
                break;
            case 1:
                cost_pe[i] = 1 + z / (1 + std::exp(-0.5 * of));
                break;
            case 2:
            default:
                cost_pe[i] = 1 + z / (1 + std::exp(-0.7 * of));
                break;
        }
    }
}

double CostModel::calc_cost(const Edge& e) const {
    if (e.cap <= 0) return 1e12;  // blocked edge
    // follow legacy cost: demand+1 to anticipate usage
    int demand = e.demand + 1;
    int cap = e.cap;
    int of = demand - cap;
    auto pe = get_cost_pe(of);

    if (selcost == 2) {
        auto dah = std::pow(e.he, 3.6) / 100.0;
        auto be = 200.0;
        return (1 + dah) * pe + be;
    }
    return pe * 10.0 + 200.0;
}

}  // namespace vlsigr



