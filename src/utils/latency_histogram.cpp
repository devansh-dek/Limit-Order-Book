#include "utils/latency_histogram.hpp"

#include <time.h>

namespace elob {

double calibrate_tsc_ghz() {
    struct timespec ts0, ts1;
    uint64_t c0 = rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &ts0);

    // Busy-wait ~10 ms so we don't sleep (which could be preempted).
    struct timespec req = {0, 10'000'000};
    nanosleep(&req, nullptr);

    uint64_t c1 = rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &ts1);

    uint64_t ns = static_cast<uint64_t>(ts1.tv_sec  - ts0.tv_sec)  * 1'000'000'000ULL
                + static_cast<uint64_t>(ts1.tv_nsec - ts0.tv_nsec);
    return static_cast<double>(c1 - c0) / static_cast<double>(ns);
}

void LatencyHistogram::sort_if_needed() {
    if (!sorted_) {
        std::sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
}

uint64_t LatencyHistogram::percentile(double p) {
    if (samples_.empty()) return 0;
    sort_if_needed();
    // Use the "nearest rank" method: rank = ceil(p/100 * N), 1-indexed.
    size_t n    = samples_.size();
    size_t rank = static_cast<size_t>(p / 100.0 * static_cast<double>(n) + 0.999999);
    if (rank < 1)  rank = 1;
    if (rank > n)  rank = n;
    return samples_[rank - 1];
}

uint64_t LatencyHistogram::min_ns() {
    if (samples_.empty()) return 0;
    sort_if_needed();
    return samples_.front();
}

uint64_t LatencyHistogram::max_ns() {
    if (samples_.empty()) return 0;
    sort_if_needed();
    return samples_.back();
}

double LatencyHistogram::mean_ns() {
    if (samples_.empty()) return 0.0;
    double sum = static_cast<double>(
        std::accumulate(samples_.begin(), samples_.end(), uint64_t{0}));
    return sum / static_cast<double>(samples_.size());
}

} // namespace elob
