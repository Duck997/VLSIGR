#include "ispd_data.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>

namespace vlsigr {

static void expect(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

IspdData parse_ispd(std::istream& is) {
    IspdData data;
    std::string keyword;

    // grid x y layer
    is >> keyword >> data.numXGrid >> data.numYGrid >> data.numLayer;
    expect(is && keyword == "grid", "failed to read grid");

    // vertical capacity
    is >> keyword >> keyword;
    expect(is && keyword == "capacity", "failed to read vertical capacity tag");
    data.verticalCapacity.clear();
    for (int i = 0; i < data.numLayer; i++) {
        int v; is >> v;
        expect(is.good(), "failed to read vertical capacity");
        data.verticalCapacity.push_back(v);
    }
    is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // horizontal capacity
    is >> keyword >> keyword;
    expect(is && keyword == "capacity", "failed to read horizontal capacity tag");
    data.horizontalCapacity.clear();
    for (int i = 0; i < data.numLayer; i++) {
        int v; is >> v;
        expect(is.good(), "failed to read horizontal capacity");
        data.horizontalCapacity.push_back(v);
    }
    is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // minimum width
    is >> keyword >> keyword;
    expect(is && keyword == "width", "failed to read minimum width tag");
    data.minimumWidth.clear();
    for (int i = 0; i < data.numLayer; i++) {
        int v; is >> v;
        expect(is.good(), "failed to read minimum width");
        data.minimumWidth.push_back(v);
    }
    is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // minimum spacing
    is >> keyword >> keyword;
    expect(is && keyword == "spacing", "failed to read minimum spacing tag");
    data.minimumSpacing.clear();
    for (int i = 0; i < data.numLayer; i++) {
        int v; is >> v;
        expect(is.good(), "failed to read minimum spacing");
        data.minimumSpacing.push_back(v);
    }
    is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // via spacing
    is >> keyword >> keyword;
    expect(is && keyword == "spacing", "failed to read via spacing tag");
    data.viaSpacing.clear();
    for (int i = 0; i < data.numLayer; i++) {
        int v; is >> v;
        expect(is.good(), "failed to read via spacing");
        data.viaSpacing.push_back(v);
    }
    is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // origin/tile
    is >> data.lowerLeftX >> data.lowerLeftY >> data.tileWidth >> data.tileHeight;
    expect(is.good(), "failed to read origin/tile size");

    // num net
    is >> keyword >> keyword >> data.numNet;
    expect(is && keyword == "net", "failed to read num net");

    data.nets.clear();
    data.nets.reserve(data.numNet);
    for (int i = 0; i < data.numNet; i++) {
        Net net;
        is >> net.name >> net.id >> net.numPins >> net.minimumWidth;
        expect(is.good(), "failed to read net header");
        net.pins.reserve(net.numPins);
        for (int j = 0; j < net.numPins; j++) {
            int x, y, z;
            is >> x >> y >> z;
            expect(is.good(), "failed to read pin");
            net.pins.emplace_back(x, y, z);
        }
        data.nets.emplace_back(std::move(net));
    }

    // capacity adjustments
    is >> data.numCapacityAdj;
    expect(is.good(), "failed to read num capacity adjustments");
    data.capacityAdjs.clear();
    data.capacityAdjs.reserve(data.numCapacityAdj);
    for (int i = 0; i < data.numCapacityAdj; i++) {
        int x1, y1, z1, x2, y2, z2, reduced;
        is >> x1 >> y1 >> z1 >> x2 >> y2 >> z2 >> reduced;
        expect(is.good(), "failed to read capacity adjustment");
        data.capacityAdjs.push_back(CapacityAdj{{x1, y1, z1}, {x2, y2, z2}, reduced});
    }

    return data;
}

IspdData parse_ispd_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) throw std::runtime_error("failed to open file: " + path);
    return parse_ispd(ifs);
}

}  // namespace vlsigr


