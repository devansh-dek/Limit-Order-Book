#include "engine/order.hpp"
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "utils/metrics.hpp"

#include <chrono>
#include <iostream>

using namespace elob;
using ns = std::chrono::nanoseconds;

int main() {
    const size_t N = 10000; // tuneable

    OrderBook book;
    MatchingEngine engine(book);
    EventIngestor ingestor(book, engine);
    Metrics m;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 1; i <= N; ++i) {
        Order o(i, (i % 2 == 0) ? Side::Buy : Side::Sell, 100 + (int64_t)(i % 5), 1 + (i % 10), (uint64_t)i);
        NewOrder no{o};
        Event ev(i, (uint64_t)i, EventPayload(no));
        auto trades = ingestor.process(ev);
        m.orders_ingested++;
        m.trades_executed += trades.size();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_ns = std::chrono::duration_cast<ns>(end - start).count();
    double secs = double(total_ns) / 1e9;
    double tput = double(N) / secs;

    std::cout << "Benchmark: N=" << N << " time=" << secs << "s throughput=" << tput << " ops/s\n";
    std::cout << "Metrics: " << m.summary() << "\n";
    return 0;
}
