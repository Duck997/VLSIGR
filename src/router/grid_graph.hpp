#pragma once

#include <cstddef>
#include <vector>

namespace vlsigr {

template<typename T>
class GridGraph {
    std::size_t w_ = 0, h_ = 0;
    std::size_t vsz_ = 0, hsz_ = 0;
    std::vector<T> edges_;

public:
    std::size_t width()  const { return w_; }
    std::size_t height() const { return h_; }

    inline std::size_t rp2idx(int x, int y, bool hori) const {
        if (hori)
            return static_cast<std::size_t>(x) * h_ + static_cast<std::size_t>(y) + vsz_;
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * w_;
    }

    const T& at(int x, int y, bool hori) const {
        return edges_.at(rp2idx(x, y, hori));
    }
    T& at(int x, int y, bool hori) {
        return edges_.at(rp2idx(x, y, hori));
    }

    void init(std::size_t width, std::size_t height, const T& vInit, const T& hInit) {
        w_ = width; h_ = height;
        vsz_ = w_ * (h_ - 1);
        hsz_ = (w_ - 1) * h_;
        edges_.clear();
        edges_.reserve(vsz_ + hsz_);
        edges_.insert(edges_.end(), vsz_, vInit);
        edges_.insert(edges_.end(), hsz_, hInit);
    }

    auto begin() { return edges_.begin(); }
    auto begin() const { return edges_.begin(); }
    auto end() { return edges_.end(); }
    auto end() const { return edges_.end(); }
};

}  // namespace vlsigr



