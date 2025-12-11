#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <limits>
#include <cstdio>

#include "router/ispd_data.hpp"

namespace {

struct EdgeAgg {
    int cap = 0;
    int demand = 0;
};

struct Cell {
    int demand = 0;
    int cap = 0;
    int via = -3;  // -1: horizontal edge, -2: vertical edge, -3: empty, >=0: via count at node
};

using EdgeGrid = std::vector<std::vector<std::vector<EdgeAgg>>>;  // [x][y][layer]

std::tuple<int,int,int,int,int,int> parse_segment(const std::string& line) {
    // format: (x1,y1,z1)-(x2,y2,z2)
    char c;
    int x1,y1,z1,x2,y2,z2;
    std::stringstream ss(line);
    ss >> c >> x1 >> c >> y1 >> c >> z1 >> c; // (x1,y1,z1)
    ss >> c;                                   // skip '-'
    ss >> c >> x2 >> c >> y2 >> c >> z2 >> c; // (x2,y2,z2)
    return {x1,y1,z1,x2,y2,z2};
}

void write_map(const std::string& path, const std::vector<std::vector<Cell>>& image, int numLayer) {
    const int ih = static_cast<int>(image.size());
    const int iw = ih ? static_cast<int>(image[0].size()) : 0;
    std::ofstream fcmap(path);
    if (!fcmap.is_open()) {
        std::cerr << "Failed to open map output: " << path << std::endl;
        return;
    }
    fcmap << numLayer << '\n';
    // Output from top to bottom
    for (int i = 0; i < ih; i++) {
        for (int j = 0; j < iw; j++) {
            const auto& e = image[i][j];
            fcmap << e.demand << '/' << e.cap << '/' << e.via << (j+1==iw ? '\n' : ' ');
        }
    }
    fcmap.close();
}

} // namespace

void write_ppm(const std::string& path, const std::vector<std::vector<Cell>>& image, int numLayer) {
    const int ih = static_cast<int>(image.size());
    const int iw = ih ? static_cast<int>(image[0].size()) : 0;
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        std::cerr << "Failed to open ppm output: " << path << std::endl;
        return;
    }
    std::fprintf(fp, "P3\n%d %d\n255\n", iw, ih);
    auto color = [&](int demand, int cap, int via, int& r, int& g, int& b) {
        if (via == -3) { // filler
            r=g=b=0; 
            return; 
        }
        if (via == -1 || via == -2) { // edge
            // Blockage/Macro: cap=0 -> black
            if (cap <= 0) {
                r = g = b = 0;
                return;
            }
            // No routing -> dark background
            if (demand == 0) {
                r = g = b = 40; // dark gray (to contrast with routed areas)
                return;
            }
            // Congestion coloring: green -> yellow -> red
            double util = static_cast<double>(demand) / cap;
            if (util <= 0.5) { // 0-50%: green to yellow
                g = 255;
                r = static_cast<int>(255 * util * 2);
                b = 0;
            } else if (util <= 1.0) { // 50%-100%: yellow to red
                r = 255;
                g = static_cast<int>(255 * (1 - util) * 2);
                b = 0;
            } else { // overflow: bright red
                r = 255; g = 0; b = 0;
            }
        } else if (via == 0) { // node without via -> light gray
            r = g = b = 230;
        } else { // node with via -> darker gray (more via = darker)
            int intensity = std::min(200, via * 20);
            r = g = b = 200 - intensity;
        }
    };
    for (int i = 0; i < ih; ++i) {
        for (int j = 0; j < iw; ++j) {
            int r=0,g=0,b=0;
            auto &e = image[i][j];
            color(e.demand, e.cap, e.via, r, g, b);
            std::fprintf(fp, "%d %d %d ", r, g, b);
        }
        std::fprintf(fp, "\n");
    }
    std::fclose(fp);
    std::cerr << "PPM saved to " << path << " (size " << iw << " x " << ih << ")\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " <input.gr> <output.txt> <map.txt> [image.ppm]\n";
        return 1;
    }
    std::string in_gr = argv[1];
    std::string in_out = argv[2];
    std::string out_map = argv[3];
    std::string out_ppm = (argc == 5) ? argv[4] : "";

    // Parse ISPD input
    vlsigr::IspdData data;
    try {
        data = vlsigr::parse_ispd_file(in_gr);
    } catch (const std::exception& e) {
        std::cerr << "Parse gr failed: " << e.what() << std::endl;
        return 1;
    }
    const int X = data.numXGrid;
    const int Y = data.numYGrid;
    const int Z = data.numLayer;
    // match router: min_net = avg(minimumWidth) + avg(minimumSpacing)
    auto avg = [](const std::vector<int>& v) {
        long long s = 0;
        for (auto x : v) s += x;
        return v.empty() ? 0 : static_cast<int>(s / static_cast<long long>(v.size()));
    };
    int min_net = avg(data.minimumWidth) + avg(data.minimumSpacing);
    if (min_net <= 0) min_net = 1;

    // Edge capacity per layer
    EdgeGrid vertical(X, std::vector<std::vector<EdgeAgg>>(Y-1, std::vector<EdgeAgg>(Z)));
    EdgeGrid horizontal(X-1, std::vector<std::vector<EdgeAgg>>(Y, std::vector<EdgeAgg>(Z)));
    for (int z = 0; z < Z; ++z) {
        int vcap = data.verticalCapacity[z] / min_net;
        int hcap = data.horizontalCapacity[z] / min_net;
        for (int x = 0; x < X; ++x)
            for (int y = 0; y < Y-1; ++y)
                vertical[x][y][z].cap = vcap;
        for (int x = 0; x < X-1; ++x)
            for (int y = 0; y < Y; ++y)
                horizontal[x][y][z].cap = hcap;
    }
    // Apply capacity adjustments
    for (const auto& adj : data.capacityAdjs) {
        auto [x1,y1,z1] = adj.grid1;
        auto [x2,y2,z2] = adj.grid2;
        if (z1 != z2) continue;
        int z = z1 - 1;
        int lx = std::min(x1, x2), rx = std::max(x1, x2);
        int ly = std::min(y1, y2), ry = std::max(y1, y2);
        int dx = rx - lx, dy = ry - ly;
        if (dx + dy != 1) continue;
        bool hori = dx;
        if (hori) {
            int cap_layer = data.horizontalCapacity[z] / min_net;
            horizontal[lx][ly][z].cap -= (cap_layer - adj.reducedCapacityLevel / min_net);
        } else {
            int cap_layer = data.verticalCapacity[z] / min_net;
            vertical[lx][ly][z].cap -= (cap_layer - adj.reducedCapacityLevel / min_net);
        }
    }

    // Accumulate demand from output segments
    std::ifstream fin(in_out);
    if (!fin.is_open()) {
        std::cerr << "Failed to open routing output: " << in_out << std::endl;
        return 1;
    }
    int seg_count = 0, via_count = 0, net_count = 0, skip_count = 0;
    while (!fin.eof()) {
        std::string netname, line;
        int id;
        if (!(fin >> netname >> id)) break;
        net_count++;
        fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        while (std::getline(fin, line)) {
            if (line == "!") break;
            if (line.empty()) continue;
            auto [x1r,y1r,z1,x2r,y2r,z2] = parse_segment(line);
            int x1 = (x1r - data.lowerLeftX) / data.tileWidth;
            int y1 = (y1r - data.lowerLeftY) / data.tileHeight;
            int x2 = (x2r - data.lowerLeftX) / data.tileWidth;
            int y2 = (y2r - data.lowerLeftY) / data.tileHeight;
            int z  = z1 - 1;
            int zt = z2 - 1;
            if (z != zt) {
                via_count++;
                continue;
            }
            if (x1 == x2 && y1 == y2) { skip_count++; continue; }
            if (z < 0 || z >= Z) { skip_count++; continue; }
            if (y1 == y2) {
                // horizontal edge
                if (x1 > x2) std::swap(x1, x2);
                for (int x = x1; x < x2; ++x) {
                    if (x < 0 || x >= X-1 || y1 < 0 || y1 >= Y) continue;
                    horizontal[x][y1][z].demand++;
                    seg_count++;
                }
            } else if (x1 == x2) {
                // vertical edge
                if (y1 > y2) std::swap(y1, y2);
                for (int y = y1; y < y2; ++y) {
                    if (x1 < 0 || x1 >= X || y < 0 || y >= Y-1) continue;
                    vertical[x1][y][z].demand++;
                    seg_count++;
                }
            }
        }
    }
    fin.close();
    std::cerr << "Parsed " << net_count << " nets, " << seg_count << " segments, " 
              << via_count << " vias, " << skip_count << " skipped from output\n";

    // Build image grid (2*X-1 by 2*Y-1), top to bottom
    int iw = 2 * X - 1;
    int ih = 2 * Y - 1;
    std::vector<std::vector<Cell>> image(ih, std::vector<Cell>(iw));
    for (int i = 0; i < ih; ++i) {
        for (int j = 0; j < iw; ++j) {
            int x = j / 2;
            int y = (ih - 1 - i) / 2; // flip vertically to match draw.cpp style
            auto& cell = image[i][j];
            switch (((i % 2) << 1) | (j % 2)) {
                case 0: { // node
                    cell.via = 0; // via count ignored here (LayerAssignment already resolved)
                    break;
                }
                case 1: { // horizontal edge
                    cell.via = -1;
                    int demand = 0, cap = 0;
                    if (x >= 0 && x < X-1 && y >= 0 && y < Y) {
                        for (int z = 0; z < Z; ++z) {
                            demand += horizontal[x][y][z].demand;
                            cap    += horizontal[x][y][z].cap;
                        }
                    }
                    cell.demand = demand;
                    cell.cap = cap;
                    break;
                }
                case 2: { // vertical edge
                    cell.via = -2;
                    int demand = 0, cap = 0;
                    if (x >= 0 && x < X && y >= 0 && y < Y-1) {
                        for (int z = 0; z < Z; ++z) {
                            demand += vertical[x][y][z].demand;
                            cap    += vertical[x][y][z].cap;
                        }
                    }
                    cell.demand = demand;
                    cell.cap = cap;
                    break;
                }
                default: { // filler
                    cell.via = -3;
                }
            }
        }
    }

    write_map(out_map, image, Z);
    std::cerr << "Map saved to " << out_map << " (size " << iw << " x " << ih << ")\n";
    if (!out_ppm.empty()) {
        write_ppm(out_ppm, image, Z);
    }
    return 0;
}

