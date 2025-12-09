#include "utils.hpp"

namespace vlsigr {

std::mt19937 rng(0);

int sign(int x) {
    return x == 0 ? 0 : (x > 0 ? 1 : -1);
}

double sec_since(std::chrono::steady_clock::time_point start) {
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

}  // namespace vlsigr



