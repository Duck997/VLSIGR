# VLSIGR

A high-performance VLSI global router implementation that incorporates adaptive algorithms and intelligent scoring mechanisms to achieve competitive routing quality in integrated circuit design.

## Quick Start

### Prerequisites
- **C++**: C++14 or later with OpenMP support
- **Python**: Python 3.7+ with Cython for C++ binding
- **Dependencies**: NumPy, Matplotlib (optional for visualization)

### Installation


### Usage


## Features

- **Multi-Mode Cost Function**: Adaptive scoring system with congestion, wirelength, and via optimization
- **History-based Updating Method (HUM)**: Iterative routing improvement with dual expansion strategy
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

### Academic Use
This project is intended for academic and educational purposes only.

## References

1. ISPD 2008 Global Routing Contest. https://www.ispd.cc/contests/08/ispd08rc.html
2. W.-H. Liu, Y.-L. Li, and C.-K. Koh. "A fast maze-free routing congestion estimator with hybrid unilateral monotonic routing." 2012 IEEE/ACM International Conference on Computer-Aided Design (ICCAD), San Jose, CA, USA, 2012, pp. 713-719.
3. W.-H. Liu, W.-C. Kao, Y.-L. Li, and K.-Y. Chao. "NCTU-GR 2.0: Multithreaded Collision-Aware Global Routing With Bounded-Length Maze Routing." IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, vol. 32, no. 5, pp. 709-722, May 2013. DOI: 10.1109/TCAD.2012.2235124
4. VLSI Physical Design Automation: Theory and Practice. Sait, Sadiq M., and Youssef, Habib.
5. Global Routing in VLSI Design. Cong, Jason, and Shinnerl, Joseph R.

## Author

**Duck997** - VLSI Global Router Implementation
