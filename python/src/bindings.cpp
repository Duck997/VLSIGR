#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "api/vlsigr.hpp"
#include "tools/draw_api.hpp"

namespace py = pybind11;

namespace {

struct PyPoint {
    int x = 0, y = 0, z = 0;
};

struct PyRPoint {
    int x = 0, y = 0, z = 0;
    bool hori = false;
};

struct PyTwoPin {
    PyPoint from;
    PyPoint to;
    std::vector<PyRPoint> path;
    int reroute = 0;
    bool overflow = false;
};

struct PyNet {
    std::string name;
    int id = 0;
    int numPins = 0;
    std::vector<PyTwoPin> twopins;
};

// Python-side results snapshot (deep-copied from router internal state).
struct PyResults {
    std::vector<PyNet> nets;
};

struct PyMetrics {
    double execution_time = 0.0;
    int total_overflow = -1;
    int max_overflow = -1;
    long long wirelength = -1;

    long long wirelength_2d = -1;
    long long wirelength_total = -1;
    long long total_vias = -1;
};

PyMetrics to_py_metrics(const vlsigr::PerformanceMetrics& m) {
    PyMetrics pm;
    pm.execution_time = m.runtime_sec;
    pm.total_overflow = m.total_overflow;
    pm.max_overflow = m.max_overflow;
    pm.wirelength_total = m.wirelength_total;
    pm.wirelength_2d = m.wirelength_2d;
    pm.total_vias = m.total_vias;
    pm.wirelength = m.wirelength_total;
    return pm;
}

PyResults snapshot_results(const vlsigr::IspdData& d) {
    PyResults r;
    r.nets.reserve(d.nets.size());
    for (const auto& net : d.nets) {
        PyNet n;
        n.name = net.name;
        n.id = net.id;
        n.numPins = net.numPins;
        n.twopins.reserve(net.twopin.size());
        for (const auto& tp : net.twopin) {
            PyTwoPin t;
            t.from = {tp.from.x, tp.from.y, tp.from.z};
            t.to = {tp.to.x, tp.to.y, tp.to.z};
            t.reroute = tp.reroute;
            t.overflow = tp.overflow;
            t.path.reserve(tp.path.size());
            for (const auto& rp : tp.path) {
                t.path.push_back({rp.x, rp.y, rp.z, rp.hori});
            }
            n.twopins.push_back(std::move(t));
        }
        r.nets.push_back(std::move(n));
    }
    return r;
}

}  // namespace

PYBIND11_MODULE(vlsigr, m) {
    m.doc() = "VLSIGR pybind11 bindings";

    py::enum_<vlsigr::Mode>(m, "Mode")
        .value("BALANCED", vlsigr::Mode::BALANCED)
        .value("CONGESTION", vlsigr::Mode::CONGESTION)
        .value("WIRELENGTH", vlsigr::Mode::WIRELENGTH)
        .export_values();

    py::class_<PyResults>(m, "Results")
        .def(py::init<>())
        .def_readonly("nets", &PyResults::nets);

    py::class_<PyPoint>(m, "Point")
        .def_readonly("x", &PyPoint::x)
        .def_readonly("y", &PyPoint::y)
        .def_readonly("z", &PyPoint::z);

    py::class_<PyRPoint>(m, "RPoint")
        .def_readonly("x", &PyRPoint::x)
        .def_readonly("y", &PyRPoint::y)
        .def_readonly("z", &PyRPoint::z)
        .def_readonly("hori", &PyRPoint::hori);

    py::class_<PyTwoPin>(m, "TwoPin")
        .def_readonly("from_", &PyTwoPin::from)
        .def_readonly("to", &PyTwoPin::to)
        .def_readonly("path", &PyTwoPin::path)
        .def_readonly("reroute", &PyTwoPin::reroute)
        .def_readonly("overflow", &PyTwoPin::overflow);

    py::class_<PyNet>(m, "Net")
        .def_readonly("name", &PyNet::name)
        .def_readonly("id", &PyNet::id)
        .def_readonly("numPins", &PyNet::numPins)
        .def_readonly("twopins", &PyNet::twopins);

    py::class_<PyMetrics>(m, "Metrics")
        .def_readonly("execution_time", &PyMetrics::execution_time)
        .def_readonly("total_overflow", &PyMetrics::total_overflow)
        .def_readonly("max_overflow", &PyMetrics::max_overflow)
        .def_readonly("wirelength", &PyMetrics::wirelength)
        .def_readonly("wirelength_2d", &PyMetrics::wirelength_2d)
        .def_readonly("wirelength_total", &PyMetrics::wirelength_total)
        .def_readonly("total_vias", &PyMetrics::total_vias);

    py::class_<vlsigr::GlobalRouter>(m, "GlobalRouter")
        .def(py::init<>())
        .def("load_ispd_benchmark", &vlsigr::GlobalRouter::load_ispd_benchmark, py::arg("path"))
        .def("set_mode",
             [](vlsigr::GlobalRouter& r, vlsigr::Mode mode) { r.setMode(mode); },
             py::arg("mode"))
        .def("enable_adaptive_scoring",
             [](vlsigr::GlobalRouter& r, bool on) { r.enableAdaptiveScoring(on); },
             py::arg("on"))
        .def("enable_hum_optimization",
             [](vlsigr::GlobalRouter& r, bool on) { r.enableHUMOptimization(on); },
             py::arg("on"))
        .def(
            "route",
            [](vlsigr::GlobalRouter& r, const std::string& output_txt) {
                r.route(output_txt);
                return snapshot_results(r.data());
            },
            py::arg("output_txt") = std::string{})
        .def(
            "get_results",
            [](const vlsigr::GlobalRouter& r) {
                return snapshot_results(r.data());
            })
        .def(
            "get_metrics",
            [](const vlsigr::GlobalRouter& r) { return to_py_metrics(r.getPerformanceMetrics()); })
        .def(
            "visualize_results",
            [](vlsigr::GlobalRouter& r,
               const PyResults& /*results*/,
               const std::string& out_ppm,
               py::object nets_ppm,
               py::object overflow_ppm,
               py::object layer_dir,
               py::object stats_path,
               py::object out_map,
               int scale) {
                vlsigr::draw::DrawOptions opt;
                opt.out_ppm = out_ppm;
                opt.scale = scale <= 0 ? 1 : scale;

                auto set_opt_str = [](py::object o, std::string& dst) {
                    if (!o.is_none()) dst = o.cast<std::string>();
                };
                set_opt_str(nets_ppm, opt.nets_ppm);
                set_opt_str(overflow_ppm, opt.overflow_ppm);
                set_opt_str(layer_dir, opt.layer_dir);
                set_opt_str(stats_path, opt.stats_path);
                set_opt_str(out_map, opt.out_map);

                vlsigr::draw::render_from_data(r.data(), opt);
            },
            py::arg("results"),
            py::arg("out_ppm"),
            py::kw_only(),
            py::arg("nets_ppm") = py::none(),
            py::arg("overflow_ppm") = py::none(),
            py::arg("layer_dir") = py::none(),
            py::arg("stats_path") = py::none(),
            py::arg("out_map") = py::none(),
            py::arg("scale") = 1)
        .def("cleanup", &vlsigr::GlobalRouter::cleanup);
}


