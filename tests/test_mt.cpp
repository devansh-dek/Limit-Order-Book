#include "elob/engine_mt.hpp"
#include "elob/order.hpp"
#include "elob/metrics.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

using namespace elob;
using Clock = std::chrono::high_resolution_clock;

int main() {
    const size_t N = 50000;
    const size_t num_threads = 4;

    // Pre-generate events deterministically
    std::vector<Event> events;
    events.reserve(N);
    for (size_t i = 1; i <= N; ++i) {
        Order o(i, (i%2?Side::Buy:Side::Sell), 100 + (int64_t)(i%5), 1 + (i%10), (uint64_t)i);
        NewOrder no{o};
        events.emplace_back(i, (uint64_t)i, EventPayload(no));
    }

    EngineMultiThreaded engine;
    std::atomic<size_t> trades_total{0};
    std::atomic<size_t> next_idx{0};

    auto worker = [&]() {
        size_t local_trades = 0;
        while (true) {
            size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
            if (idx >= N) break;
            auto trades = engine.process_event(events[idx]);
            local_trades += trades.size();
        }
        trades_total.fetch_add(local_trades, std::memory_order_relaxed);
    };

    auto start = Clock::now();
    std::vector<std::thread> workers;
    for (size_t i = 0; i < num_threads; ++i) workers.emplace_back(worker);
    for (auto &t : workers) t.join();
    auto end = Clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    double tput = double(N) / secs;

    std::cout << "Multi-threaded benchmark: N=" << N << " threads=" << num_threads
              << " time=" << secs << "s throughput=" << tput << " ops/s\n";
    std::cout << "Total trades: " << trades_total.load() << "\n";
    return 0;
}
