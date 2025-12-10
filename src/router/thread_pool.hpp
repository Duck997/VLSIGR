#pragma once

#include <cstddef>
#include <memory>
#include <thread>

#include "ThreadPool.h"

namespace vlsigr {

inline ThreadPool& thread_pool() {
    static std::unique_ptr<ThreadPool> pool =
        std::make_unique<ThreadPool>(std::thread::hardware_concurrency());
    return *pool;
}

inline void set_thread_pool(std::unique_ptr<ThreadPool> p) {
    static std::unique_ptr<ThreadPool>& pool_ref = []() -> std::unique_ptr<ThreadPool>& {
        static std::unique_ptr<ThreadPool> pool =
            std::make_unique<ThreadPool>(std::thread::hardware_concurrency());
        return pool;
    }();
    pool_ref = std::move(p);
}

}  // namespace vlsigr


