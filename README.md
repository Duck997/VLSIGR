# VLSIGR

A high-performance VLSI global router implementation that incorporates adaptive algorithms and intelligent scoring mechanisms to achieve competitive routing quality in integrated circuit design.

## Quick Start

### Prerequisites
- **C++**: C++17 or later (multi-threading via header-only [progschj/ThreadPool](https://github.com/progschj/ThreadPool))
- **Python**: Python 3.7+ with Cython for C++ binding
- **Dependencies**: NumPy, Matplotlib (optional for visualization)

### Installation
- Build (router + draw):  
  ```bash
  make
  ```
- Build tests (if有):  
  ```bash
  make test
  ```

### Usage
CLI：
```bash
# 只跑 routing，不輸出 layer assignment
./router examples/complex.gr

# 跑 routing 並啟動 Layer Assignment，結果寫出 output.txt
./router examples/complex.gr output.txt

# 視覺化（congestion / nets）
./draw examples/complex.gr output.txt examples/complex_map.txt examples/complex.ppm --nets examples/complex_nets.ppm --scale 3
```

#### C++ API

```cpp
#include "api/vlsigr.hpp"
using namespace vlsigr;

int main() {
    GlobalRouter router;
    router.load_ispd_benchmark("examples/complex.gr");
    router.route("output.txt"); // 可選：若提供路徑，會同時跑 LayerAssignment 並寫出結果

    const auto& m = router.getPerformanceMetrics();
    // m.runtime_sec, m.total_overflow, m.max_overflow
    // m.wirelength_2d, m.wirelength_total, m.total_vias
    return 0;
}
```

```cpp
#include "api/vlsigr.hpp"

int main() {
    auto* data = ISPDParser::parse_file("examples/complex.gr"); // new; caller owns

    VLSIGR::GlobalRouting router;
    router.init(*data);
    router.setMode(VLSIGR::Mode::BALANCED);
    router.enableAdaptiveScoring(true);
    router.enableHUMOptimization(true);
    router.route();

    auto results = router.getResults();
    auto metrics = router.getPerformanceMetrics();

    VLSIGR::Visualization viz;
    viz.generateMap(data, results, "routing_result.ppm");

    delete data;
    return 0;
}
```

#### API Visualization (advanced)
`generateMap()` 也提供進階 overload，可以用 `vlsigr::draw::DrawOptions` 打開 `draw.cpp` 的完整功能（`nets/overflow/layers/stats/scale/...`）：

```cpp
VLSIGR::Visualization viz;
vlsigr::draw::DrawOptions opt;
opt.nets_ppm = "complex_nets.ppm";
opt.overflow_ppm = "complex_overflow.ppm";
opt.layer_dir = "layers";
opt.stats_path = "stats.txt";
opt.out_map = "complex_map.txt";
opt.scale = 3;
viz.generateMap(&router.data(), results, "complex.ppm", opt);
```

#### Tests
- 快速跑所有單元測試：

```bash
make test
```

- `adaptec1.gr` 驗證（官方 `eval2008.pl`，較慢，預設不跑）：

```bash
VLSIGR_RUN_ADAPTEC1=1 make test
```

## Benchmark Results (ISPD 2008)

**Environment**
- **CPU**: 13th Gen Intel(R) Core(TM) i7-13700HX (8 logical CPUs)
- **Memory**: 12 GiB
- **OS**: Ubuntu 22.04

| Benchmarks | TOF | MOF | WL | Runtime (sec) |
|---|---:|---:|---:|---:|
| adaptec1 | 0 | 0 | 6609421 | 201.49 |
| adaptec2 | 0 | 0 | 5851083 | 74.95 |
| adaptec3 | 0 | 0 | 14750808 | 288.24 |
| adaptec4 | 0 | 0 | 12446724 | 93.14 |
| adaptec5 | 0 | 0 | 17905064 | 410.28 |
| bigblue1 | 0 | 0 | 7259427 | 328.64 |
| bigblue2 | 120 | 2 | 10611910 | 1294.62 |
| bigblue3 | 0 | 0 | 13758934 | 160.13 |
| newblue1 | 34 | 2 | 5423906 | 1677.29 |
| newblue2 | 0 | 0 | 7703791 | 40.87 |
| newblue5 | 0 | 0 | 26484667 | 518.35 |
| newblue6 | 0 | 0 | 20798696 | 406.46 |

## Features

- **Multi-Mode Cost Function**: Adaptive scoring system with congestion, wirelength, and via optimization
- **Hybrid Unilateral Monotonic (HUM)**: Iterative routing improvement with dual expansion strategy
- **Pattern Routing**: Initial routing with L-shape and Z-shape patterns
- **Layer Assignment**: Multi-layer routing with via optimization
- **Visualization Tools**: PPM format output with congestion color coding
- **Cross-Platform Support**: Compatible with different operating systems and compilers

## License

This project contains code from multiple sources:

### Original Work
- Core routing algorithms and optimizations: Original implementation
- Project documentation and analysis: Original work

### Third-Party Components
- **LayerAssignment Module**: Copyright (c) 2013 by Wen-Hao Liu and Yih-Lang Li
  - URL: http://cs.nycu.edu.tw/~whliu/NCTU-GR.htm
  - License: Academic research use only
- **ThreadPool**: Header-only C++11 thread pool by Jakob Progsch
  - URL: https://github.com/progschj/ThreadPool
  - License: zlib license

### Academic Use
This project is intended for academic and educational purposes only.

## References

1. ISPD 2008 Global Routing Contest. https://www.ispd.cc/contests/08/ispd08rc.html
2. W.-H. Liu, Y.-L. Li, and C.-K. Koh. "A fast maze-free routing congestion estimator with hybrid unilateral monotonic routing." 2012 IEEE/ACM International Conference on Computer-Aided Design (ICCAD), San Jose, CA, USA, 2012, pp. 713-719.
3. W.-H. Liu, W.-C. Kao, Y.-L. Li, and K.-Y. Chao. "NCTU-GR 2.0: Multithreaded Collision-Aware Global Routing With Bounded-Length Maze Routing." IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, vol. 32, no. 5, pp. 709-722, May 2013. DOI: 10.1109/TCAD.2012.2235124
4. VLSI Physical Design Automation: Theory and Practice. Sait, Sadiq M., and Youssef, Habib.
5. Global Routing in VLSI Design. Cong, Jason, and Shinnerl, Joseph R.
6. Jakob Progsch. "ThreadPool" (C++11 thread pool), https://github.com/progschj/ThreadPool (zlib license).

## Author

**Duck997** - VLSI Global Router Implementation
