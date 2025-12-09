#pragma once

// Pattern routing interfaces (L-shape, Z-shape, monotonic).
// These functions populate TwoPin::path with Manhattan edges (RPoint).
// Cost-aware variants accept an optional cost functor; if not provided, unit cost is used.

#pragma once

#include <functional>

#include "router/ispd_data.hpp"

namespace vlsigr::patterns {

// Compute L-shape path (pick cheaper of the two bends; tie-break randomly via rng).
void Lshape(TwoPin& tp, const std::function<double(int,int,bool)>& cost_fn = {});

// Compute Z-shape path using dynamic programming over a bounding box.
void Zshape(TwoPin& tp, const std::function<double(int,int,bool)>& cost_fn = {});

// Monotonic (Manhattan shortest) path with cost tie-breaking.
void Monotonic(TwoPin& tp, const std::function<double(int,int,bool)>& cost_fn = {});

}  // namespace vlsigr::patterns



