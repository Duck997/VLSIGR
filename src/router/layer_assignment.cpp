#include "layer_assignment.hpp"

#include <memory>
#include <iostream>

#include "../../third_party/LayerAssignment.h"
#include "../../third_party/ispdData.h"

namespace vlsigr {

namespace {

std::unique_ptr<ISPDParser::ispdData> to_legacy(const IspdData& d) {
    auto legacy = std::make_unique<ISPDParser::ispdData>();
    legacy->numXGrid = d.numXGrid;
    legacy->numYGrid = d.numYGrid;
    legacy->numLayer = d.numLayer;
    legacy->verticalCapacity = d.verticalCapacity;
    legacy->horizontalCapacity = d.horizontalCapacity;
    legacy->minimumWidth = d.minimumWidth;
    legacy->minimumSpacing = d.minimumSpacing;
    legacy->viaSpacing = d.viaSpacing;
    legacy->lowerLeftX = d.lowerLeftX;
    legacy->lowerLeftY = d.lowerLeftY;
    legacy->tileWidth = d.tileWidth;
    legacy->tileHeight = d.tileHeight;
    legacy->numNet = d.numNet;
    legacy->numCapacityAdj = d.numCapacityAdj;
    // capacity adj
    for (auto& adj : d.capacityAdjs) {
        auto* ca = new ISPDParser::CapacityAdj();
        ca->grid1 = adj.grid1;
        ca->grid2 = adj.grid2;
        ca->reducedCapacityLevel = adj.reducedCapacityLevel;
        legacy->capacityAdjs.push_back(ca);
    }
    // nets
    for (auto& n : d.nets) {
        auto* net = new ISPDParser::Net(n.name, n.id, n.numPins, n.minimumWidth);
        net->pins = n.pins;
        net->pin2D.reserve(n.pin2D.size());
        for (auto& p : n.pin2D) net->pin2D.emplace_back(p.x, p.y, p.z);
        net->pin3D.reserve(n.pin3D.size());
        for (auto& p : n.pin3D) net->pin3D.emplace_back(p.x, p.y, p.z);
        net->twopin.reserve(n.twopin.size());
        for (auto& tp : n.twopin) {
            ISPDParser::TwoPin ltp;
            ltp.from = ISPDParser::Point(tp.from.x, tp.from.y, tp.from.z);
            ltp.to   = ISPDParser::Point(tp.to.x, tp.to.y, tp.to.z);
            ltp.parNet = net;
            ltp.reroute = tp.reroute;
            ltp.overflow = tp.overflow;
            ltp.ripup = tp.ripup;
            // Copy path before push_back (TwoPin copy ctor doesn't copy path!)
            for (auto& rp : tp.path) {
                ltp.path.emplace_back(rp.x, rp.y, rp.z, rp.hori);
            }
            net->twopin.push_back(ltp);
            // CRITICAL: TwoPin copy ctor doesn't copy path, so copy it again!
            net->twopin.back().path = ltp.path;
        }
        legacy->nets.push_back(net);
    }
    return legacy;
}

}  // namespace

LayerAssignmentResult run_layer_assignment(IspdData& data,
                                           const std::string& output_path,
                                           bool print_to_screen) {
    auto legacy = to_legacy(data);
    
    // Debug: check twopin path conversion
    if (print_to_screen) {
        std::cerr << "[DEBUG] to_legacy: " << legacy->nets.size() << " nets" << std::endl;
        for (size_t i = 0; i < std::min(legacy->nets.size(), (size_t)3); ++i) {
            auto* net = legacy->nets[i];
            std::cerr << "  net[" << i << "] " << net->name << " has " 
                      << net->twopin.size() << " twopins" << std::endl;
            for (size_t j = 0; j < std::min(net->twopin.size(), (size_t)2); ++j) {
                auto& tp = net->twopin[j];
                std::cerr << "    twopin[" << j << "] path.size=" << tp.path.size() << std::endl;
            }
        }
    }
    
    LayerAssignment::Graph graph;
    graph.initialLA(*legacy, 1);
    graph.convertGRtoLA(*legacy, print_to_screen);
    graph.COLA(print_to_screen);
    if (!output_path.empty()) {
        graph.output3Dresult(output_path.c_str());
    }
    LayerAssignmentResult res;
    res.totalOF = graph.totalOF;
    res.maxOF = graph.maxOF;
    res.totalVia = graph.totalVia;
    res.wlen2D = graph.origiWL;
    res.via = graph.totalVia;
    res.totalWL = graph.origiWL + graph.totalVia;
    return res;
}

}  // namespace vlsigr


