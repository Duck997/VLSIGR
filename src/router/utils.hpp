#pragma once

#include <chrono>
#include <random>
#include <vector>

namespace vlsigr {

extern std::mt19937 rng;

int sign(int x);

double sec_since(std::chrono::steady_clock::time_point start);

template<typename T>
T randint(T l, T r) { return std::uniform_int_distribution<T>(l, r)(rng); }

template<typename T>
T randint(T n) { return randint<T>(0, n - 1); }

template<typename T>
inline T average(const std::vector<T>& v) {
    T acc{};
    for (const auto& x : v) acc += x;
    return acc / static_cast<T>(v.size());
}

}  // namespace vlsigr



