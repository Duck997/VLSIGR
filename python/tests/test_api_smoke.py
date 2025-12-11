import os
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def test_python_api_smoke_complex(tmp_path: Path):
    import vlsigr

    gr = repo_root() / "examples" / "complex.gr"
    assert gr.exists()

    router = vlsigr.GlobalRouter()
    router.load_ispd_benchmark(str(gr))

    router.set_mode(vlsigr.Mode.BALANCED)
    router.enable_adaptive_scoring(True)
    router.enable_hum_optimization(True)

    results = router.route("")  # no LA output in this smoke test
    metrics = router.get_metrics()

    assert metrics.execution_time >= 0.0
    assert metrics.total_overflow >= -1

    out_ppm = tmp_path / "routing_result.ppm"
    router.visualize_results(results, str(out_ppm))
    assert out_ppm.exists()

    # PPM header sanity
    with out_ppm.open("r") as f:
        magic = f.readline().strip()
        assert magic == "P3"

    router.cleanup()


def test_python_api_adaptec1_optional(tmp_path: Path):
    # Optional (slow) test: enable explicitly.
    if os.environ.get("VLSIGR_RUN_ADAPTEC1") != "1":
        return

    import vlsigr

    gr = Path(os.environ.get("VLSIGR_ADAPTEC1_GR", str(repo_root() / "examples" / "adaptec1.gr")))
    if not gr.exists():
        return

    router = vlsigr.GlobalRouter()
    router.load_ispd_benchmark(str(gr))
    results = router.route(str(tmp_path / "adaptec1_output.txt"))
    m = router.get_metrics()
    # With LA output, metrics should be populated.
    assert m.total_overflow >= 0

    out_ppm = tmp_path / "adaptec1.ppm"
    router.visualize_results(results, str(out_ppm), scale=1)
    assert out_ppm.exists()


