#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <numeric>

namespace elob {

// Read the CPU timestamp counter. ~1–5 cycle overhead; no syscall.
// Used for nanosecond-resolution per-event latency measurement.
// calibrate_tsc_ghz() converts raw cycles to nanoseconds.
static inline uint64_t rdtsc() noexcept {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// Measure TSC frequency by comparing rdtsc against CLOCK_MONOTONIC over ~10 ms.
// Returns cycles per nanosecond (e.g. 3.2 on a 3.2 GHz CPU).
// Call once at program start; the result is stable for the session.
double calibrate_tsc_ghz();

// Exact-percentile histogram backed by a sorted vector.
// Suitable for offline benchmarking (collect N samples, query once).
// For online/low-latency use, prefer HdrHistogram or power-of-2 buckets.
class LatencyHistogram {
public:
    explicit LatencyHistogram(size_t reserve = 65536) { samples_.reserve(reserve); }

    void record(uint64_t ns) { samples_.push_back(ns); sorted_ = false; }

    // p in [0, 100]. Returns 0 if empty.
    uint64_t percentile(double p);

    uint64_t p50()  { return percentile(50.0);  }
    uint64_t p99()  { return percentile(99.0);  }
    uint64_t p999() { return percentile(99.9);  }

    uint64_t min_ns();
    uint64_t max_ns();
    double   mean_ns();
    size_t   count()  const noexcept { return samples_.size(); }
    void     reset()        noexcept { samples_.clear(); sorted_ = false; }

private:
    void sort_if_needed();
    std::vector<uint64_t> samples_;
    bool sorted_ = false;
};

} // namespace elob
