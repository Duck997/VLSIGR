#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <chrono>

#include "router/ispd_data.hpp"
#include "router/routing_core.hpp"
#include "router/layer_assignment.hpp"
#include "router/utils.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <input.gr> [output.txt]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    std::string input_file = argv[1];
    std::string output_file = (argc >= 3) ? argv[2] : "";
    
    vlsigr::IspdData data;
    try {
        data = vlsigr::parse_ispd_file(input_file);
        std::cerr << "[INFO] Parsed input '" << input_file << "'" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "[ERROR] Parse failed: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    vlsigr::RoutingCore router;
    std::cerr << "[*] parsing done, start routing..." << std::endl;
    
    try {
        router.route(data, false);
    } catch (bool done) {
        if (done) {
            std::cerr << "[INFO] Routing converged to 0 overflow!" << std::endl;
        }
    } catch (...) {
        std::cerr << "[ERROR] Routing failed" << std::endl;
        return EXIT_FAILURE;
    }
    
    std::cerr << "[INFO] Routing completed" << std::endl;
    
    if (!output_file.empty()) {
        // Debug: check path state before LA
        std::cerr << "[DEBUG] Before LA: checking paths" << std::endl;
        for (std::size_t i = 0; i < std::min(data.nets.size(), (std::size_t)3); ++i) {
            auto& net = data.nets[i];
            std::cerr << "  net[" << i << "] " << net.name << " has " 
                      << net.twopin.size() << " twopins" << std::endl;
            for (std::size_t j = 0; j < std::min(net.twopin.size(), (std::size_t)2); ++j) {
                auto& tp = net.twopin[j];
                std::cerr << "    twopin[" << j << "] path.size=" << tp.path.size() << std::endl;
            }
        }
        
        std::cerr << "[*] Starting Layer Assignment -> " << output_file << std::endl;
        auto la_start = std::chrono::steady_clock::now();
        auto res = vlsigr::run_layer_assignment(data, output_file, true);
        std::cerr << "[INFO] LA done in " << vlsigr::sec_since(la_start) << "s"
                  << " totalOF=" << res.totalOF
                  << " maxOF=" << res.maxOF
                  << " totalVia=" << res.totalVia
                  << " WLen2D=" << res.wlen2D
                  << " totalWL=" << res.totalWL << std::endl;
    }
    
    return EXIT_SUCCESS;
}
