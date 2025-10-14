#include "utils/engine_lockfree.hpp"
#include "utils/engine_mt.hpp"
#include "engine/order.hpp"
#include "data/event.hpp"
#include "utils/metrics.hpp"

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>

using namespace elob;

// Generate deterministic test events
std::vector<Event> generate_events(size_t N) {
    std::vector<Event> events;
    events.reserve(N);

    // Alternating buys and sells at different prices
    for (size_t i = 0; i < N; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        int64_t price = 10000 + (i % 100);  // Prices: 10000-10099
        uint64_t qty = 100;
        uint64_t order_id = i + 1;
        uint64_t timestamp = i;

        Order order{order_id, side, price, qty, timestamp};
        Event event;
        event.payload = NewOrder{order};
        events.push_back(event);
    }

    return events;
}

// Benchmark lock-free engine
double benchmark_lockfree(const std::vector<Event>& events) {
    EngineLockFree engine;
    engine.start();

    auto start = std::chrono::high_resolution_clock::now();

    // Submit all events
    for (const auto& event : events) {
        while (!engine.submit(event)) {
            // Queue full, retry
            std::this_thread::yield();
        }
    }

    // Wait for all events to be processed
    engine.drain();

    auto end = std::chrono::high_resolution_clock::now();

    engine.stop();

    std::chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

// Benchmark mutex-based engine
double benchmark_mutex(const std::vector<Event>& events) {
    EngineMultiThreaded engine;

    auto start = std::chrono::high_resolution_clock::now();

    // Process all events (need to make mutable copy for API)
    for (auto event : events) {
        engine.process_event(event);
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

// Benchmark single-threaded (baseline)
double benchmark_singlethread(const std::vector<Event>& events) {
    OrderBook book;
    MatchingEngine matcher(book);
    EventIngestor ingestor(book, matcher);

    auto start = std::chrono::high_resolution_clock::now();

    for (auto event : events) {
        ingestor.process(event);
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

void run_comparison(size_t N) {
    std::cout << "Generating " << N << " events..." << std::endl;
    auto events = generate_events(N);

    std::cout << "\n=== Benchmarking (N=" << N << ") ===" << std::endl;

    // Single-threaded baseline
    std::cout << "Running single-threaded baseline..." << std::flush;
    double st_time = benchmark_singlethread(events);
    double st_throughput = N / st_time;
    std::cout << " done" << std::endl;

    // Mutex-based concurrent
    std::cout << "Running mutex-based concurrent..." << std::flush;
    double mutex_time = benchmark_mutex(events);
    double mutex_throughput = N / mutex_time;
    std::cout << " done" << std::endl;

    // Lock-free concurrent
    std::cout << "Running lock-free concurrent..." << std::flush;
    double lf_time = benchmark_lockfree(events);
    double lf_throughput = N / lf_time;
    std::cout << " done" << std::endl;

    // Results
    std::cout << "\n--- Results ---" << std::endl;
    std::cout << std::fixed << std::setprecision(6);

    std::cout << "\nSingle-threaded:" << std::endl;
    std::cout << "  Time:       " << st_time << " s" << std::endl;
    std::cout << "  Throughput: " << std::setprecision(0) << st_throughput << " ops/s" << std::endl;

    std::cout << "\nMutex-based:" << std::endl;
    std::cout << "  Time:       " << std::setprecision(6) << mutex_time << " s" << std::endl;
    std::cout << "  Throughput: " << std::setprecision(0) << mutex_throughput << " ops/s" << std::endl;
    std::cout << "  Speedup:    " << std::setprecision(2) << (st_time / mutex_time) << "x" << std::endl;

    std::cout << "\nLock-free:" << std::endl;
    std::cout << "  Time:       " << std::setprecision(6) << lf_time << " s" << std::endl;
    std::cout << "  Throughput: " << std::setprecision(0) << lf_throughput << " ops/s" << std::endl;
    std::cout << "  Speedup:    " << std::setprecision(2) << (st_time / lf_time) << "x" << std::endl;

    std::cout << "\nLock-free vs Mutex:" << std::endl;
    std::cout << "  Improvement: " << std::setprecision(2) << (mutex_time / lf_time) << "x" << std::endl;

    std::cout << std::endl;
}

int main() {
    std::cout << "Lock-Free vs Mutex-Based Engine Comparison" << std::endl;
    std::cout << "===========================================" << std::endl;

    // Run multiple sizes
    std::vector<size_t> sizes = {1000, 10000, 50000, 100000};

    for (size_t N : sizes) {
        run_comparison(N);
    }

    std::cout << "Benchmark complete!" << std::endl;
    return 0;
}
