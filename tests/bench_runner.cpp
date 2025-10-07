#include "elob/order.hpp"
#include "elob/order_book.hpp"
#include "elob/matching_engine.hpp"
#include "elob/ingestor.hpp"
#include "elob/metrics.hpp"

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace elob;
using Clock = std::chrono::high_resolution_clock;

struct Result { std::string scenario; size_t N; double seconds; double throughput; uint64_t trades; uint64_t orders; };

// Scenario generators: produce a vector of Orders deterministically given seed
static std::vector<Order> gen_same_price(size_t N) {
    std::vector<Order> v; v.reserve(N);
    for (size_t i = 1; i <= N; ++i) v.emplace_back((uint64_t)i, (i%2?Side::Buy:Side::Sell), 100, 1 + (i%5), (uint64_t)i);
    return v;
}

static std::vector<Order> gen_spread(size_t N) {
    std::vector<Order> v; v.reserve(N);
    for (size_t i = 1; i <= N; ++i) v.emplace_back((uint64_t)i, (i%2?Side::Buy:Side::Sell), 80 + (int64_t)(i%41), 1 + (i%10), (uint64_t)i);
    return v;
}

static std::vector<Order> gen_crossing(size_t N) {
    std::vector<Order> v; v.reserve(N);
    // create many opposing orders so many trades occur
    for (size_t i = 1; i <= N; ++i) {
        if (i % 3 == 0) v.emplace_back((uint64_t)i, Side::Sell, 100 + (int64_t)(i%3), 1 + (i%4), (uint64_t)i);
        else v.emplace_back((uint64_t)i, Side::Buy, 100 + (int64_t)(i%3), 1 + (i%4), (uint64_t)i);
    }
    return v;
}

Result run_scenario(const std::string &name, size_t N, const std::vector<Order> &orders) {
    OrderBook book;
    MatchingEngine engine(book);
    EventIngestor ingestor(book, engine);
    Metrics m;

    auto start = Clock::now();
    for (const auto &o : orders) {
        NewOrder no{o};
        Event ev(o.order_id, o.timestamp, EventPayload(no));
        auto trades = ingestor.process(ev);
        m.orders_ingested++;
        m.trades_executed += trades.size();
    }
    auto end = Clock::now();
    double secs = std::chrono::duration<double>(end - start).count();
    double tput = double(N) / secs;
    return Result{name, N, secs, tput, m.trades_executed, m.orders_ingested};
}

int main() {
    std::vector<Result> results;
    std::vector<size_t> Ns = {1000, 10000, 50000};

    for (size_t N : Ns) {
        results.push_back(run_scenario("same_price", N, gen_same_price(N)));
        results.push_back(run_scenario("spread", N, gen_spread(N)));
        results.push_back(run_scenario("crossing", N, gen_crossing(N)));
    }

    // write CSV
    std::ofstream ofs("bench_results.csv");
    ofs << "scenario,N,seconds,throughput_ops_s,trades,orders\n";
    for (auto &r : results) {
        ofs << r.scenario << ',' << r.N << ',' << r.seconds << ',' << r.throughput << ',' << r.trades << ',' << r.orders << '\n';
    }
    ofs.close();

    std::cout << "Wrote bench_results.csv with " << results.size() << " rows\n";
    return 0;
}
