#include "engine/order.hpp"
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "utils/metrics.hpp"
#include "utils/latency_histogram.hpp"

#include <chrono>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace elob;
using Clock = std::chrono::high_resolution_clock;

struct Result {
    std::string scenario;
    size_t   N;
    double   seconds;
    double   throughput;
    uint64_t trades;
    uint64_t orders;
    uint64_t p50_ns;
    uint64_t p99_ns;
    uint64_t p999_ns;
    uint64_t min_ns;
    uint64_t max_ns;
};

static std::vector<Order> gen_same_price(size_t N) {
    std::vector<Order> v; v.reserve(N);
    for (size_t i = 1; i <= N; ++i)
        v.emplace_back((uint64_t)i, (i%2?Side::Buy:Side::Sell), 100, 1+(i%5), (uint64_t)i);
    return v;
}

static std::vector<Order> gen_spread(size_t N) {
    std::vector<Order> v; v.reserve(N);
    for (size_t i = 1; i <= N; ++i)
        v.emplace_back((uint64_t)i, (i%2?Side::Buy:Side::Sell), 80+(int64_t)(i%41), 1+(i%10), (uint64_t)i);
    return v;
}

static std::vector<Order> gen_crossing(size_t N) {
    std::vector<Order> v; v.reserve(N);
    for (size_t i = 1; i <= N; ++i) {
        if (i % 3 == 0) v.emplace_back((uint64_t)i, Side::Sell, 100+(int64_t)(i%3), 1+(i%4), (uint64_t)i);
        else            v.emplace_back((uint64_t)i, Side::Buy,  100+(int64_t)(i%3), 1+(i%4), (uint64_t)i);
    }
    return v;
}

Result run_scenario(const std::string& name, size_t N,
                    const std::vector<Order>& orders, double tsc_ghz) {
    OrderBook      book;
    MatchingEngine engine(book);
    EventIngestor  ingestor(book, engine);
    Metrics        m;
    LatencyHistogram hist(N);

    auto start = Clock::now();
    for (const auto& o : orders) {
        NewOrder no{o};
        Event ev(o.order_id, o.timestamp, EventPayload(no));

        uint64_t t0     = rdtsc();
        auto     trades = ingestor.process(ev);
        uint64_t t1     = rdtsc();

        uint64_t ns = static_cast<uint64_t>(
            static_cast<double>(t1 - t0) / tsc_ghz);
        hist.record(ns);

        m.orders_ingested++;
        m.trades_executed += trades.size();
    }
    auto   end  = Clock::now();
    double secs = std::chrono::duration<double>(end - start).count();

    return Result{name, N, secs, double(N)/secs,
                  m.trades_executed, m.orders_ingested,
                  hist.p50(), hist.p99(), hist.p999(),
                  hist.min_ns(), hist.max_ns()};
}

int main() {
    std::cout << "Calibrating TSC... ";
    double tsc_ghz = calibrate_tsc_ghz();
    std::cout << std::fixed << std::setprecision(3) << tsc_ghz << " GHz\n\n";

    std::vector<Result> results;
    for (size_t N : {1000UL, 10000UL, 50000UL}) {
        results.push_back(run_scenario("same_price", N, gen_same_price(N), tsc_ghz));
        results.push_back(run_scenario("spread",     N, gen_spread(N),     tsc_ghz));
        results.push_back(run_scenario("crossing",   N, gen_crossing(N),   tsc_ghz));
    }

    // Print table
    std::cout << std::left
              << std::setw(12) << "scenario"
              << std::setw(8)  << "N"
              << std::setw(14) << "ops/s"
              << std::setw(10) << "p50 ns"
              << std::setw(10) << "p99 ns"
              << std::setw(10) << "p999 ns"
              << std::setw(10) << "min ns"
              << std::setw(10) << "max ns"
              << '\n'
              << std::string(84, '-') << '\n';
    for (auto& r : results) {
        std::cout << std::left
                  << std::setw(12) << r.scenario
                  << std::setw(8)  << r.N
                  << std::setw(14) << std::llround(r.throughput)
                  << std::setw(10) << r.p50_ns
                  << std::setw(10) << r.p99_ns
                  << std::setw(10) << r.p999_ns
                  << std::setw(10) << r.min_ns
                  << std::setw(10) << r.max_ns
                  << '\n';
    }

    // Write CSV
    std::ofstream ofs("bench_results.csv");
    ofs << "scenario,N,seconds,throughput_ops_s,trades,orders,"
           "p50_ns,p99_ns,p999_ns,min_ns,max_ns\n";
    for (auto& r : results) {
        ofs << r.scenario << ',' << r.N << ',' << r.seconds << ','
            << r.throughput << ',' << r.trades << ',' << r.orders << ','
            << r.p50_ns << ',' << r.p99_ns << ',' << r.p999_ns << ','
            << r.min_ns << ',' << r.max_ns << '\n';
    }
    std::cout << "\nWrote bench_results.csv\n";
    return 0;
}
