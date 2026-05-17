#include "utils/latency_histogram.hpp"

#include <cstdio>
#include <cstdlib>

using namespace elob;

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); std::exit(1); }
}

static void test_empty() {
    LatencyHistogram h;
    check(h.count()    == 0, "empty: count = 0");
    check(h.percentile(50) == 0, "empty: p50 = 0");
    check(h.min_ns()   == 0, "empty: min = 0");
    check(h.max_ns()   == 0, "empty: max = 0");
}

static void test_single_sample() {
    LatencyHistogram h;
    h.record(42);
    check(h.count()  == 1,  "single: count = 1");
    check(h.p50()    == 42, "single: p50 = 42");
    check(h.p99()    == 42, "single: p99 = 42");
    check(h.p999()   == 42, "single: p999 = 42");
    check(h.min_ns() == 42, "single: min = 42");
    check(h.max_ns() == 42, "single: max = 42");
}

static void test_sorted_percentiles() {
    // 100 samples: 1, 2, ..., 100
    LatencyHistogram h;
    for (uint64_t i = 1; i <= 100; ++i) h.record(i);

    // p50 = sample at rank ceil(50) = 50 → value 50
    check(h.p50()  == 50,  "sort: p50 = 50");
    // p99 = sample at rank ceil(99) = 99 → value 99
    check(h.p99()  == 99,  "sort: p99 = 99");
    // p999 = sample at rank ceil(99.9) = 100 → value 100
    check(h.p999() == 100, "sort: p999 = 100");
    check(h.min_ns() == 1,   "sort: min = 1");
    check(h.max_ns() == 100, "sort: max = 100");
}

static void test_insertion_order_irrelevant() {
    // Insert 1–100 in reverse; percentiles must be the same.
    LatencyHistogram h;
    for (uint64_t i = 100; i >= 1; --i) h.record(i);

    check(h.p50()  == 50,  "order: p50 = 50");
    check(h.p99()  == 99,  "order: p99 = 99");
    check(h.min_ns() == 1,   "order: min = 1");
    check(h.max_ns() == 100, "order: max = 100");
}

static void test_reset() {
    LatencyHistogram h;
    h.record(100);
    h.record(200);
    check(h.count() == 2, "reset: count = 2 before reset");
    h.reset();
    check(h.count()  == 0, "reset: count = 0 after reset");
    check(h.p50()    == 0, "reset: p50 = 0 after reset");
}

static void test_tail_latency() {
    // 1000 samples: 990×10 ns (fast), 10×10000 ns (slow).
    // Sorted: indices 0–989=10, indices 990–999=10000.
    // p99  → rank=990 → index 989 → 10   (just inside the fast bucket)
    // p999 → rank=999 → index 998 → 10000 (inside the slow bucket)
    LatencyHistogram h;
    for (int i = 0; i < 990; ++i) h.record(10);
    for (int i = 0; i < 10;  ++i) h.record(10000);

    check(h.p99()  == 10,    "tail: p99 = 10 (below outliers)");
    check(h.p999() == 10000, "tail: p999 = 10000 (inside outlier bucket)");
}

static void test_rdtsc_runs() {
    // rdtsc should return strictly increasing values across two calls.
    uint64_t a = rdtsc();
    uint64_t b = rdtsc();
    check(b >= a, "rdtsc: second call >= first call");
}

static void test_mean() {
    LatencyHistogram h;
    h.record(10);
    h.record(20);
    h.record(30);
    double m = h.mean_ns();
    check(m > 19.9 && m < 20.1, "mean: mean of 10,20,30 = 20");
}

int main() {
    test_empty();
    test_single_sample();
    test_sorted_percentiles();
    test_insertion_order_irrelevant();
    test_reset();
    test_tail_latency();
    test_rdtsc_runs();
    test_mean();

    std::puts("All histogram tests passed.");
    return 0;
}
