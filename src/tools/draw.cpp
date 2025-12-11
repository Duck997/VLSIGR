#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <map>

#include "router/ispd_data.hpp"

namespace {

struct EdgeAgg {
    int cap = 0;
    int demand = 0;
};

struct Cell {
    int demand = 0;
    int cap = 0;
    int via = -3;  // -1: horizontal edge, -2: vertical edge, -3: filler, >=0: via count at node
    std::set<int> nets;  // all net IDs passing through this cell
    bool blockage = false;
    int x = 0, y = 0;  // grid coordinates for debugging
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

// Main PPM output (congestion-colored)
void write_ppm(const std::string& path, const std::vector<std::vector<Cell>>& image, int numLayer, int scale = 1) {
    const int ih = static_cast<int>(image.size());
    const int iw = ih ? static_cast<int>(image[0].size()) : 0;
    (void)numLayer;
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        std::cerr << "Failed to open ppm output: " << path << std::endl;
        return;
    }
    std::fprintf(fp, "P3\n%d %d\n255\n", iw * scale, ih * scale);
    
    // Lambda to compute cell color
    auto get_cell_color = [&](const Cell& cell, int& r, int& g, int& b) {
        if (cell.blockage) {
            r = 0; g = 255; b = 255; // bright cyan
        } else if (cell.via == -1 || cell.via == -2) {
            // Edge
            if (cell.cap <= 0) {
                r = g = b = 0;
            } else if (cell.demand == 0) {
                r = g = b = 18; // darker background
            } else {
                double util = static_cast<double>(cell.demand) / cell.cap;
                if (util <= 0.5) {
                    g = 255; r = static_cast<int>(255 * util * 2); b = 0;
                } else if (util <= 1.0) {
                    r = 255; g = static_cast<int>(255 * (1 - util) * 2); b = 0;
                } else {
                    r = 255; g = 0; b = 0;
                }
            }
        } else {
            // Node
            if (cell.nets.empty()) {
                r = g = b = 18; // match background to avoid grid
            } else {
                r = g = b = 140; // darker but still visible
            }
        }
    };
    
    for (int i = 0; i < ih; ++i) {
        for (int si = 0; si < scale; ++si) {
            for (int j = 0; j < iw; ++j) {
                const auto& cell = image[i][j];
                int r=22, g=22, b=22;
                
                if (cell.via == -3) {
                    // Filler: match unused edge color for seamless background
                    r = g = b = 18;
                } else {
                    get_cell_color(cell, r, g, b);
                }
                
                bool over = (cell.via == -1 || cell.via == -2) && cell.cap > 0 && cell.demand > cell.cap;
                for (int sj = 0; sj < scale; ++sj) {
                    if (over && (si == sj || si + sj == scale - 1)) {
                        std::fprintf(fp, "%d %d %d ", 80, 80, 80); // gray X
                    } else {
                        std::fprintf(fp, "%d %d %d ", r, g, b);
                    }
                }
            }
            std::fprintf(fp, "\n");
        }
    }
    std::fclose(fp);
    std::cerr << "PPM saved to " << path << " (size " << iw * scale << " x " << ih * scale << ", scale=" << scale << ")\n";
}

// Only highlight overflow edges (red) on dark background
void write_overflow_ppm(const std::string& path, const std::vector<std::vector<Cell>>& image, int scale = 1) {
    const int ih = static_cast<int>(image.size());
    const int iw = ih ? static_cast<int>(image[0].size()) : 0;
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        std::cerr << "Failed to open overflow ppm output: " << path << std::endl;
        return;
    }
    std::fprintf(fp, "P3\n%d %d\n255\n", iw * scale, ih * scale);
    
    for (int i = 0; i < ih; ++i) {
        for (int si = 0; si < scale; ++si) {
            for (int j = 0; j < iw; ++j) {
                const auto& e = image[i][j];
                int r=12, g=12, b=12;
                
                if (e.blockage) {
                    r = 0; g = 255; b = 200;
                } else if ((e.via == -1 || e.via == -2) && e.cap > 0 && e.demand > e.cap) {
                    r = 255; g = 0; b = 0;
                } else if ((e.via == -1 || e.via == -2) && e.cap <= 0) {
                    r = g = b = 0;
                }
                
                bool over = (e.via == -1 || e.via == -2) && e.cap > 0 && e.demand > e.cap;
                for (int sj = 0; sj < scale; ++sj) {
                    if (over && (si == sj || si + sj == scale - 1)) {
                        std::fprintf(fp, "%d %d %d ", 80, 80, 80);
                    } else {
                        std::fprintf(fp, "%d %d %d ", r, g, b);
                    }
                }
            }
            std::fprintf(fp, "\n");
        }
    }
    std::fclose(fp);
    std::cerr << "Overflow mask saved to " << path << " (size " << iw * scale << " x " << ih * scale << ", scale=" << scale << ")\n";
}

// Color by net ID (each net uses distinct color from palette)
void write_nets_ppm(const std::string& path,
                    const std::vector<std::vector<Cell>>& image,
                    int scale = 1) {
    const int ih = static_cast<int>(image.size());
    const int iw = ih ? static_cast<int>(image[0].size()) : 0;
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        std::cerr << "Failed to open nets ppm output: " << path << std::endl;
        return;
    }
    std::fprintf(fp, "P3\n%d %d\n255\n", iw * scale, ih * scale);
    
    const int palette[][3] = {
        {31,119,180},  // blue
        {255,127,14},  // orange
        {44,160,44},   // green
        {214,39,40},   // red
        {148,103,189}, // purple
        {140,86,75},   // brown
        {227,119,194}, // pink
        {188,189,34}   // olive
    };
    constexpr int palette_sz = 8;
    
    // Enhance color saturation for better visibility
    auto enhance = [](int c) { return std::min(255, static_cast<int>(c * 1.15)); };
    
    // Lambda to get color for a cell
    auto get_cell_color = [&](const Cell& cell, int& r, int& g, int& b) -> bool {
        if (cell.blockage) {
            // Bright cyan for high visibility
            r = 0; g = 255; b = 255;
            return true;
        } else if (cell.via == -1 || cell.via == -2) {
            // Edge: enhanced saturation for better visibility
            if (cell.cap <= 0) {
                r = g = b = 0;
                return true;
            } else if (cell.nets.empty()) {
                r = g = b = 18; // darker background
                return false; // unused
            } else {
                int net_id = *cell.nets.begin();
                auto idx = net_id % palette_sz;
                r = enhance(palette[idx][0]);
                g = enhance(palette[idx][1]);
                b = enhance(palette[idx][2]);
                return true; // has net
            }
        } else if (cell.via >= 0) {
            // Node: slightly darker but still vibrant
            if (cell.nets.empty()) {
                r = g = b = 18; // align with background to remove grid
                return false; // unused
            } else if (cell.nets.size() == 1) {
                int net_id = *cell.nets.begin();
                auto idx = net_id % palette_sz;
                r = static_cast<int>(enhance(palette[idx][0]) * 0.85);
                g = static_cast<int>(enhance(palette[idx][1]) * 0.85);
                b = static_cast<int>(enhance(palette[idx][2]) * 0.85);
                return true; // has net
            } else {
                r = g = b = 120;
                return true; // multiple nets
            }
        }
        return false;
    };
    
    for (int i = 0; i < ih; ++i) {
        for (int si = 0; si < scale; ++si) {
            for (int j = 0; j < iw; ++j) {
                const auto& cell = image[i][j];
                int r=22, g=22, b=22; // default background
                
                if (cell.via == -3) {
                    // Filler: stay background unless a net passes straight through (vertical or horizontal)
                    auto get_color_and_nets = [&](int ni, int nj, int& tr, int& tg, int& tb) -> std::set<int> {
                        if (ni >= 0 && ni < ih && nj >= 0 && nj < iw && image[ni][nj].via != -3) {
                            get_cell_color(image[ni][nj], tr, tg, tb);
                            return image[ni][nj].nets;
                        }
                        tr = tg = tb = 0;
                        return {};
                    };

                    int ru, gu, bu, rd, gd, bd, rl, gl, bl, rr, gr, br;
                    auto up    = get_color_and_nets(i-1, j, ru, gu, bu);
                    auto down  = get_color_and_nets(i+1, j, rd, gd, bd);
                    auto left  = get_color_and_nets(i, j-1, rl, gl, bl);
                    auto right = get_color_and_nets(i, j+1, rr, gr, br);

                    auto shared_net = [](const std::set<int>& a, const std::set<int>& b) -> bool {
                        for (int net : a) if (b.count(net)) return true;
                        return false;
                    };

                    bool vert_pass = !up.empty() && !down.empty() && shared_net(up, down);
                    bool hori_pass = !left.empty() && !right.empty() && shared_net(left, right);

                    if (vert_pass) {
                        r = static_cast<int>((ru + rd) * 0.55); // average then darken 0.55*2 = 1.1 ≈ full
                        g = static_cast<int>((gu + gd) * 0.55);
                        b = static_cast<int>((bu + bd) * 0.55);
                    } else if (hori_pass) {
                        r = static_cast<int>((rl + rr) * 0.55);
                        g = static_cast<int>((gl + gr) * 0.55);
                        b = static_cast<int>((bl + br) * 0.55);
                    } else {
                        r = g = b = 18; // background
                    }
                } else {
                    get_cell_color(cell, r, g, b);
                }
                
                // All cells use same scale for consistent output
                for (int sj = 0; sj < scale; ++sj) {
                    std::fprintf(fp, "%d %d %d ", r, g, b);
                }
            }
            std::fprintf(fp, "\n");
        }
    }
    std::fclose(fp);
    std::cerr << "Net-colored PPM saved to " << path << " (size " << iw * scale << " x " << ih * scale << ", scale=" << scale << ")\n";
}

// Render one specific layer
void write_layer_ppm(const std::string& path,
                     int layer,
                     int X, int Y,
                     const EdgeGrid& vertical,
                     const EdgeGrid& horizontal,
                     const std::map<std::tuple<int,int,int,bool>, std::set<int>>& edge_nets,
                     const std::map<std::tuple<int,int>, std::set<int>>& node_nets,
                     const std::vector<std::vector<Cell>>& image_full,
                     int scale = 1) {
    (void)edge_nets;
    (void)node_nets;
    const int iw = 2 * X - 1;
    const int ih = 2 * Y - 1;
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        std::cerr << "Failed to open layer ppm output: " << path << std::endl;
        return;
    }
    std::fprintf(fp, "P3\n%d %d\n255\n", iw * scale, ih * scale);
    
    for (int i = 0; i < ih; ++i) {
        for (int si = 0; si < scale; ++si) {
            for (int j = 0; j < iw; ++j) {
                int x = j / 2;
                int y = (ih - 1 - i) / 2;
                int tag = ((i % 2) << 1) | (j % 2);
                int r=12, g=12, b=12;
                
                if (tag == 0) {
                    // Node
                    if (image_full[i][j].blockage) {
                        r = 0; g = 255; b = 200;
                    } else {
                        r = g = b = 100;
                    }
                } else if (tag == 1) {
                    // Horizontal edge
                    if (x >= 0 && x < X-1 && y >= 0 && y < Y) {
                        int demand = horizontal[x][y][layer].demand;
                        int cap = horizontal[x][y][layer].cap;
                        if (cap <= 0) { r = g = b = 0; }
                        else if (demand == 0) { r = g = b = 35; }
                        else {
                            double util = static_cast<double>(demand) / cap;
                            if (util <= 0.5) {
                                g = 255; r = static_cast<int>(255 * util * 2); b = 0;
                            } else if (util <= 1.0) {
                                r = 255; g = static_cast<int>(255 * (1 - util) * 2); b = 0;
                            } else {
                                r = 255; g = 0; b = 0;
                            }
                        }
                    }
                } else if (tag == 2) {
                    // Vertical edge
                    if (x >= 0 && x < X && y >= 0 && y < Y-1) {
                        int demand = vertical[x][y][layer].demand;
                        int cap = vertical[x][y][layer].cap;
                        if (cap <= 0) { r = g = b = 0; }
                        else if (demand == 0) { r = g = b = 35; }
                        else {
                            double util = static_cast<double>(demand) / cap;
                            if (util <= 0.5) {
                                g = 255; r = static_cast<int>(255 * util * 2); b = 0;
                            } else if (util <= 1.0) {
                                r = 255; g = static_cast<int>(255 * (1 - util) * 2); b = 0;
                            } else {
                                r = 255; g = 0; b = 0;
                            }
                        }
                    }
                }
                
                for (int sj = 0; sj < scale; ++sj) {
                    std::fprintf(fp, "%d %d %d ", r, g, b);
                }
            }
            std::fprintf(fp, "\n");
        }
    }
    std::fclose(fp);
    std::cerr << "Layer " << (layer+1) << " PPM saved to " << path << " (size " << iw * scale << " x " << ih * scale << ", scale=" << scale << ")\n";
}

struct Stats {
    double min = 0, p50 = 0, p90 = 0, p95 = 0, p99 = 0, max = 0;
    std::size_t edges = 0;
    std::size_t overflow_edges = 0;
};

double percentile(std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    double idx = p * (v.size() - 1);
    std::size_t lo = static_cast<std::size_t>(idx);
    std::size_t hi = std::min(v.size() - 1, lo + 1);
    double frac = idx - lo;
    std::nth_element(v.begin(), v.begin() + lo, v.end());
    double lo_val = v[lo];
    if (hi == lo) return lo_val;
    std::nth_element(v.begin() + lo + 1, v.begin() + hi, v.end());
    double hi_val = v[hi];
    return lo_val + (hi_val - lo_val) * frac;
}

Stats compute_stats(const EdgeGrid& vertical, const EdgeGrid& horizontal) {
    std::vector<double> util;
    std::size_t overflow_edges = 0;
    auto push_edge = [&](int demand, int cap) {
        if (cap <= 0) return;
        util.push_back(static_cast<double>(demand) / cap);
        if (demand > cap) overflow_edges++;
    };
    int X = static_cast<int>(vertical.size());
    int Y = static_cast<int>(vertical[0].size() + 1);
    int Z = static_cast<int>(vertical[0][0].size());
    for (int z = 0; z < Z; ++z) {
        for (int x = 0; x < X; ++x) {
            for (int y = 0; y < Y-1; ++y) {
                push_edge(vertical[x][y][z].demand, vertical[x][y][z].cap);
            }
        }
        for (int x = 0; x < X-1; ++x) {
            for (int y = 0; y < Y; ++y) {
                push_edge(horizontal[x][y][z].demand, horizontal[x][y][z].cap);
            }
        }
    }
    Stats s;
    s.edges = util.size();
    s.overflow_edges = overflow_edges;
    if (!util.empty()) {
        s.min = *std::min_element(util.begin(), util.end());
        s.max = *std::max_element(util.begin(), util.end());
        s.p50 = percentile(util, 0.5);
        s.p90 = percentile(util, 0.9);
        s.p95 = percentile(util, 0.95);
        s.p99 = percentile(util, 0.99);
    }
    return s;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input.gr> <output.txt> <map.txt> [image.ppm]"
                  << " [--overflow overflow.ppm] [--layers dir] [--stats stats.txt] [--nets nets.ppm] [--scale N]\n";
        return 1;
    }
    std::string in_gr = argv[1];
    std::string in_out = argv[2];
    std::string out_map = argv[3];
    std::string out_ppm;
    std::string overflow_ppm, layer_dir, stats_path, nets_ppm;
    int scale = 1;

    int arg_idx = 4;
    if (arg_idx < argc && argv[arg_idx][0] != '-') {
        out_ppm = argv[arg_idx++];
    }

    for (; arg_idx < argc; ++arg_idx) {
        std::string arg = argv[arg_idx];
        if (arg == "--overflow" && arg_idx + 1 < argc) {
            overflow_ppm = argv[++arg_idx];
        } else if (arg == "--layers" && arg_idx + 1 < argc) {
            layer_dir = argv[++arg_idx];
        } else if (arg == "--stats" && arg_idx + 1 < argc) {
            stats_path = argv[++arg_idx];
        } else if (arg == "--nets" && arg_idx + 1 < argc) {
            nets_ppm = argv[++arg_idx];
        } else if (arg == "--scale" && arg_idx + 1 < argc) {
            scale = std::stoi(argv[++arg_idx]);
        } else {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            return 1;
        }
    }

    if (!layer_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(layer_dir, ec);
        if (ec) {
            std::cerr << "Failed to create layer dir: " << layer_dir << " (" << ec.message() << ")\n";
            return 1;
        }
    }

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
    
    // Maps to track which nets use which edges/nodes
    std::map<std::tuple<int,int,int,bool>, std::set<int>> edge_nets; // (x,y,z,hori) -> net IDs
    std::map<std::tuple<int,int>, std::set<int>> node_nets; // (x,y) -> net IDs (across all layers)
    
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
    std::vector<std::tuple<int, int, int, int, int, int>> blockages;
    // Blocked edges aggregated across layers (for visualization)
    std::vector<std::vector<bool>> blocked_hori(X-1, std::vector<bool>(Y, false));   // edge (x,y) between (x,y)-(x+1,y)
    std::vector<std::vector<bool>> blocked_vert(X,   std::vector<bool>(Y-1, false)); // edge (x,y) between (x,y)-(x,y+1)
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
            if (adj.reducedCapacityLevel <= 0) {
                blockages.emplace_back(lx, ly, z, lx+1, ly, z);
                if (lx >= 0 && lx < X-1 && ly >= 0 && ly < Y) blocked_hori[lx][ly] = true;
            }
        } else {
            int cap_layer = data.verticalCapacity[z] / min_net;
            vertical[lx][ly][z].cap -= (cap_layer - adj.reducedCapacityLevel / min_net);
            if (adj.reducedCapacityLevel <= 0) {
                blockages.emplace_back(lx, ly, z, lx, ly+1, z);
                if (lx >= 0 && lx < X && ly >= 0 && ly < Y-1) blocked_vert[lx][ly] = true;
            }
        }
    }

    // Accumulate demand from output segments AND track net usage
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
                // Mark both nodes as using this net
                node_nets[{x1, y1}].insert(id);
                node_nets[{x2, y2}].insert(id);
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
                    edge_nets[{x, y1, z, true}].insert(id);
                    // Mark BOTH nodes adjacent to this edge so long segments become continuous.
                    node_nets[{x, y1}].insert(id);
                    node_nets[{x + 1, y1}].insert(id);
                    seg_count++;
                }
                // (endpoints already covered by the per-edge marking above)
            } else if (x1 == x2) {
                // vertical edge
                if (y1 > y2) std::swap(y1, y2);
                for (int y = y1; y < y2; ++y) {
                    if (x1 < 0 || x1 >= X || y < 0 || y >= Y-1) continue;
                    vertical[x1][y][z].demand++;
                    edge_nets[{x1, y, z, false}].insert(id);
                    // Mark BOTH nodes adjacent to this edge so long segments become continuous.
                    node_nets[{x1, y}].insert(id);
                    node_nets[{x1, y + 1}].insert(id);
                    seg_count++;
                }
                // (endpoints already covered by the per-edge marking above)
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
            int y = (ih - 1 - i) / 2;
            auto& cell = image[i][j];
            cell.x = x; cell.y = y;
            switch (((i % 2) << 1) | (j % 2)) {
                case 0: { // node
                    cell.via = 0;
                    if (node_nets.count({x, y})) {
                        cell.nets = node_nets[{x, y}];
                    }
                    // Node is blockage if any incident blocked edge exists
                    bool b = false;
                    if (x > 0 && x-1 < X-1 && y >= 0 && y < Y && blocked_hori[x-1][y]) b = true;
                    if (x < X-1 && y >= 0 && y < Y && blocked_hori[x][y]) b = true;
                    if (y > 0 && x >= 0 && x < X && y-1 < Y-1 && blocked_vert[x][y-1]) b = true;
                    if (y < Y-1 && x >= 0 && x < X && blocked_vert[x][y]) b = true;
                    cell.blockage = b;
                    break;
                }
                case 1: { // horizontal edge
                    cell.via = -1;
                    int demand = 0, cap = 0;
                    if (x >= 0 && x < X-1 && y >= 0 && y < Y) {
                        for (int z = 0; z < Z; ++z) {
                            demand += horizontal[x][y][z].demand;
                            cap    += horizontal[x][y][z].cap;
                            if (edge_nets.count({x, y, z, true})) {
                                for (int net_id : edge_nets[{x, y, z, true}]) {
                                    cell.nets.insert(net_id);
                                }
                            }
                        }
                    }
                    cell.demand = demand;
                    cell.cap = cap;
                    // Mark blockage edge (aggregated across layers)
                    if (x >= 0 && x < X-1 && y >= 0 && y < Y && blocked_hori[x][y]) cell.blockage = true;
                    break;
                }
                case 2: { // vertical edge
                    cell.via = -2;
                    int demand = 0, cap = 0;
                    if (x >= 0 && x < X && y >= 0 && y < Y-1) {
                        for (int z = 0; z < Z; ++z) {
                            demand += vertical[x][y][z].demand;
                            cap    += vertical[x][y][z].cap;
                            if (edge_nets.count({x, y, z, false})) {
                                for (int net_id : edge_nets[{x, y, z, false}]) {
                                    cell.nets.insert(net_id);
                                }
                            }
                        }
                    }
                    cell.demand = demand;
                    cell.cap = cap;
                    // Mark blockage edge (aggregated across layers)
                    if (x >= 0 && x < X && y >= 0 && y < Y-1 && blocked_vert[x][y]) cell.blockage = true;
                    break;
                }
                default: { // filler
                    cell.via = -3;
                }
            }
        }
    }

    // Fill in blockage continuity for fillers: if adjacent to any blockage edge/node, mark as blockage.
    for (int i = 0; i < ih; ++i) {
        for (int j = 0; j < iw; ++j) {
            auto& c = image[i][j];
            if (c.via != -3) continue; // only fillers
            bool nb = false;
            if (i > 0 && image[i-1][j].blockage) nb = true;
            if (i+1 < ih && image[i+1][j].blockage) nb = true;
            if (j > 0 && image[i][j-1].blockage) nb = true;
            if (j+1 < iw && image[i][j+1].blockage) nb = true;
            c.blockage = nb;
        }
    }

    write_map(out_map, image, Z);
    std::cerr << "Map saved to " << out_map << " (size " << iw << " x " << ih << ")\n";
    if (!out_ppm.empty()) {
        write_ppm(out_ppm, image, Z, scale);
    }
    if (!overflow_ppm.empty()) {
        write_overflow_ppm(overflow_ppm, image, scale);
    }
    if (!layer_dir.empty()) {
        for (int z = 0; z < Z; ++z) {
            std::stringstream ss;
            ss << layer_dir << "/layer_" << (z+1) << ".ppm";
            write_layer_ppm(ss.str(), z, X, Y, vertical, horizontal, edge_nets, node_nets, image, scale);
        }
    }
    if (!nets_ppm.empty()) {
        write_nets_ppm(nets_ppm, image, scale);
    }
    Stats st = compute_stats(vertical, horizontal);
    auto emit_stats = [&](std::ostream& os) {
        os << "Edges: " << st.edges
           << " overflow_edges: " << st.overflow_edges << '\n'
           << "util min/median/p90/p95/p99/max: "
           << st.min << ' ' << st.p50 << ' ' << st.p90 << ' '
           << st.p95 << ' ' << st.p99 << ' ' << st.max << '\n';
    };
    if (!stats_path.empty()) {
        std::ofstream ofs(stats_path);
        if (!ofs.is_open()) {
            std::cerr << "Failed to open stats output: " << stats_path << "\n";
        } else {
            emit_stats(ofs);
            std::cerr << "Stats saved to " << stats_path << "\n";
        }
    } else {
        emit_stats(std::cerr);
    }
    return 0;
}
