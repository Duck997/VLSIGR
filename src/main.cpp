#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>

#include "router/ispd_data.hpp"
#include "router/routing_core.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <input.gr> [max_iter]\n", argv[0]);
        return EXIT_FAILURE;
    }
    std::string input = argv[1];
    int max_iter = (argc >= 3) ? std::atoi(argv[2]) : 10;

    vlsigr::IspdData data;
    try {
        data = vlsigr::parse_ispd_file(input);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Parse failed: %s\n", e.what());
        return EXIT_FAILURE;
    }

    vlsigr::RoutingCore rc;
    std::fprintf(stderr, "[*] parsing done, start routing...\n");
    rc.preroute(data);
    rc.route_pipeline(data);            // multi-stage
    rc.route_iterate(data, max_iter);   // extra iterations if needed
    auto st = rc.check_overflow(data);

    std::fprintf(stderr, "[result] overflow_tot=%d mx=%d wl=%d\n", st.tot, st.mx, st.wl);
    // TODO: emit ISPD route format when 3D output is ready.
    return (st.tot == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
