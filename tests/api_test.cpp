#include <gtest/gtest.h>

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

#include "api/vlsigr.hpp"

namespace {

std::string repo_path(const std::string& rel) {
    // Tests are run from repo root by Makefile (./gtest). Keep paths relative.
    return rel;
}

}  // namespace

TEST(ApiSmoke, LoadAndRouteComplex) {
    const std::string gr = repo_path("examples/complex.gr");
    if (!std::filesystem::exists(gr)) {
        GTEST_SKIP() << "Missing test input: " << gr;
    }

    vlsigr::GlobalRouter router;
    ASSERT_NO_THROW(router.load_ispd_benchmark(gr));
    ASSERT_NO_THROW(router.route(""));

    const auto& res = router.getResults();
    ASSERT_NE(res.data, nullptr);
    ASSERT_EQ(res.data, &router.data());

    const auto& m = router.getPerformanceMetrics();
    EXPECT_GE(m.runtime_sec, 0.0);
    // We only guarantee these are computed when LayerAssignment runs; for pure 2D route they are approximations.
    EXPECT_GE(m.total_overflow, -1);
    EXPECT_GE(m.wirelength_2d, -1);

    // Ensure routing produced at least one non-empty path.
    bool any_path = false;
    for (const auto& net : router.data().nets) {
        for (const auto& tp : net.twopin) {
            if (!tp.path.empty()) {
                any_path = true;
                break;
            }
        }
        if (any_path) break;
    }
    EXPECT_TRUE(any_path);
}

TEST(ApiSmoke, InitThenRoute) {
    const std::string gr = repo_path("examples/complex.gr");
    if (!std::filesystem::exists(gr)) {
        GTEST_SKIP() << "Missing test input: " << gr;
    }

    auto data = vlsigr::parse_ispd_file(gr);

    vlsigr::GlobalRouter router;
    router.init(std::move(data));
    ASSERT_NO_THROW(router.route(""));

    EXPECT_NE(router.getResults().data, nullptr);
}

TEST(ApiSmoke, GenerateMapComplex) {
    const std::string gr = repo_path("examples/complex.gr");
    if (!std::filesystem::exists(gr)) {
        GTEST_SKIP() << "Missing test input: " << gr;
    }

    vlsigr::GlobalRouter router;
    ASSERT_NO_THROW(router.load_ispd_benchmark(gr));
    ASSERT_NO_THROW(router.route(""));

    VLSIGR::Visualization viz;
    VLSIGR::Results results;
    results.data = &router.data();

    const auto tmp = std::filesystem::temp_directory_path();
    const std::string ppm_path = (tmp / "vlsigr_complex.ppm").string();

    ASSERT_NO_THROW(viz.generateMap(&router.data(), results, ppm_path));
    ASSERT_TRUE(std::filesystem::exists(ppm_path));

    // Basic sanity: must be a P3 PPM with expected dimensions (2*X-1, 2*Y-1).
    std::ifstream ifs(ppm_path);
    ASSERT_TRUE(ifs.is_open());
    std::string magic;
    int w = 0, h = 0, maxv = 0;
    ifs >> magic >> w >> h >> maxv;
    EXPECT_EQ(magic, "P3");
    EXPECT_EQ(w, 2 * router.data().numXGrid - 1);
    EXPECT_EQ(h, 2 * router.data().numYGrid - 1);
    EXPECT_EQ(maxv, 255);

    std::error_code ec;
    std::filesystem::remove(ppm_path, ec);
}

TEST(ApiSmoke, Adaptec1Optional) {
    // This test is intentionally gated: adaptec1 is large and can take minutes.
    // Run it explicitly:
    //   VLSIGR_RUN_ADAPTEC1=1 make test
    const char* run = std::getenv("VLSIGR_RUN_ADAPTEC1");
    if (!(run && std::string(run) == "1")) {
        GTEST_SKIP() << "Set VLSIGR_RUN_ADAPTEC1=1 to run the adaptec1 API + eval2008 test.";
    }

    const char* env = std::getenv("VLSIGR_ADAPTEC1_GR");
    std::string gr = env ? std::string(env) : repo_path("examples/adaptec1.gr");

    if (!std::filesystem::exists(gr)) {
        GTEST_SKIP() << "Missing adaptec1.gr. Set VLSIGR_ADAPTEC1_GR or put it at examples/adaptec1.gr";
    }

    const std::string eval = repo_path("third_party/eval2008.pl");
    if (!std::filesystem::exists(eval)) {
        GTEST_SKIP() << "Missing eval script: " << eval;
    }

    auto shell_quote = [](const std::string& s) -> std::string {
        // Safe single-quote for /bin/sh: wrap in '...' and escape any embedded single quotes.
        std::string out;
        out.reserve(s.size() + 2);
        out.push_back('\'');
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out.push_back(c);
        }
        out.push_back('\'');
        return out;
    };

    auto run_capture = [&](const std::string& cmd) -> std::pair<int, std::string> {
        std::string out;
        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) return {127, ""};
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), fp)) out += buf;
        int rc = pclose(fp);
        // rc is wait status; for our use (0/non-0) this is enough.
        return {rc, out};
    };

    vlsigr::GlobalRouter router;
    ASSERT_NO_THROW(router.load_ispd_benchmark(gr));

    // Generate a routed result file via LayerAssignment (the format eval2008 expects).
    const auto tmp = std::filesystem::temp_directory_path();
    const std::string out_path = (tmp / "vlsigr_adaptec1_output.txt").string();
    ASSERT_NO_THROW(router.route(out_path));
    ASSERT_TRUE(std::filesystem::exists(out_path));

    ASSERT_NE(router.getResults().data, nullptr);

    // Use the official ISPD 2008 eval script to check overflow.
    // It prints a table with Tot OF / Max OF / WL. We require Tot OF == 0 and Max OF == 0.
    const std::string cmd =
        "perl " + shell_quote(eval) + " " + shell_quote(gr) + " " + shell_quote(out_path) + " 2>&1";
    auto [rc, text] = run_capture(cmd);
    ASSERT_EQ(rc, 0) << "eval2008.pl failed. Output:\n" << text;

    // Parse the last line that ends with three integers: TotOF MaxOF WL
    std::smatch m;
    std::regex re(R"((\d+)\s+(\d+)\s+(\d+)\s*$)", std::regex::ECMAScript);
    ASSERT_TRUE(std::regex_search(text, m, re)) << "Failed to parse eval output. Output:\n" << text;
    const int tot_of = std::stoi(m[1].str());
    const int max_of = std::stoi(m[2].str());
    EXPECT_EQ(tot_of, 0) << "Tot OF != 0. Output:\n" << text;
    EXPECT_EQ(max_of, 0) << "Max OF != 0. Output:\n" << text;
}


