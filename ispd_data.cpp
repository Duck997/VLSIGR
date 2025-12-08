#include "ispdData.h"

#include <climits>
#include <string>
#include <tuple>
#include <vector>

// ISPD 2008 official parser (reference implementation)
// This mirrors the original contest-provided reader that consumes
// the benchmark format into ISPDParser::ispdData.

namespace ISPDParser {

ispdData* parse(std::istream& is) {
    ispdData* data = new ispdData();
    std::string keyword;
    is >> keyword >> data->numXGrid >> data->numYGrid >> data->numLayer;

    is >> keyword >> keyword;  // vertical capacity
    data->verticalCapacity.clear();
    int val;
    for (int i = 0; i < data->numLayer; i++) {
        is >> val;
        data->verticalCapacity.push_back(val);
    }
    is.ignore(INT_MAX, '\n');

    is >> keyword >> keyword;  // horizontal capacity
    data->horizontalCapacity.clear();
    for (int i = 0; i < data->numLayer; i++) {
        is >> val;
        data->horizontalCapacity.push_back(val);
    }
    is.ignore(INT_MAX, '\n');

    is >> keyword >> keyword;  // minimum width
    data->minimumWidth.clear();
    for (int i = 0; i < data->numLayer; i++) {
        is >> val;
        data->minimumWidth.push_back(val);
    }
    is.ignore(INT_MAX, '\n');

    is >> keyword >> keyword;  // minimum spacing
    data->minimumSpacing.clear();
    for (int i = 0; i < data->numLayer; i++) {
        is >> val;
        data->minimumSpacing.push_back(val);
    }
    is.ignore(INT_MAX, '\n');

    is >> keyword >> keyword;  // via spacing
    data->viaSpacing.clear();
    for (int i = 0; i < data->numLayer; i++) {
        is >> val;
        data->viaSpacing.push_back(val);
    }
    is.ignore(INT_MAX, '\n');

    is >> data->lowerLeftX >> data->lowerLeftY >> data->tileWidth >> data->tileHeight;
    is >> keyword >> keyword >> data->numNet;

    data->nets.clear();
    for (int i = 0; i < data->numNet; i++) {
        std::string net_name;
        int id, num_pins, min_width;
        is >> net_name >> id >> num_pins >> min_width;
        Net* net = new Net{net_name, id, num_pins, min_width};
        for (int j = 0; j < num_pins; j++) {
            int x, y, z;
            is >> x >> y >> z;
            net->pins.push_back(std::make_tuple(x, y, z));
        }
        data->nets.push_back(net);
    }

    is >> data->numCapacityAdj;
    data->capacityAdjs.clear();
    for (int i = 0; i < data->numCapacityAdj; i++) {
        int x1, y1, z1, x2, y2, z2, reduced_capacity_level;
        is >> x1 >> y1 >> z1 >> x2 >> y2 >> z2 >> reduced_capacity_level;
        CapacityAdj* adj = new CapacityAdj{{x1, y1, z1}, {x2, y2, z2}, reduced_capacity_level};
        data->capacityAdjs.push_back(adj);
    }
    return data;
}

}  // namespace ISPDParser


