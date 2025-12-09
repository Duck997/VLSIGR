#pragma once

#include <istream>
#include <string>
#include <tuple>
#include <vector>

namespace vlsigr {

struct Point {
    int x = 0, y = 0, z = 0;
    Point() = default;
    Point(int x_, int y_, int z_ = 0): x(x_), y(y_), z(z_) {}
};

struct RPoint {
    int x = 0, y = 0, z = 0;
    bool hori = false;
    RPoint() = default;
    RPoint(int x_, int y_, bool h): x(x_), y(y_), z(0), hori(h) {}
    RPoint(int x_, int y_, int z_, bool h): x(x_), y(y_), z(z_), hori(h) {}
};

struct TwoPin {
    Point from, to;
    std::vector<RPoint> path;
    int reroute = 0;   // track how many times this twopin has been rerouted
    bool overflow = false;
};

struct Net {
    std::string name;
    int id = 0;
    int numPins = 0;
    int minimumWidth = 0;
    std::vector<std::tuple<int, int, int>> pins;
    std::vector<Point> pin2D;
    std::vector<Point> pin3D;
    std::vector<TwoPin> twopin;

    // stats
    int overflow = 0;
    int overflow_twopin = 0;
    int wlen = 0;
    double cost = 0.0;
};

struct CapacityAdj {
    std::tuple<int, int, int> grid1;
    std::tuple<int, int, int> grid2;
    int reducedCapacityLevel = 0;
};

struct IspdData {
    int numXGrid = 0;
    int numYGrid = 0;
    int numLayer = 0;

    std::vector<int> verticalCapacity;
    std::vector<int> horizontalCapacity;
    std::vector<int> minimumWidth;
    std::vector<int> minimumSpacing;
    std::vector<int> viaSpacing;

    int lowerLeftX = 0;
    int lowerLeftY = 0;
    int tileWidth = 0;
    int tileHeight = 0;

    int numNet = 0;
    std::vector<Net> nets;

    int numCapacityAdj = 0;
    std::vector<CapacityAdj> capacityAdjs;
};

// Parse ISPD 2008 format from stream; throws std::runtime_error on malformed input.
IspdData parse_ispd(std::istream& is);

// Convenience helper to load from file path.
IspdData parse_ispd_file(const std::string& path);

}  // namespace vlsigr



