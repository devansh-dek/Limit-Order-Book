// Benchmark using realistic events from CSV files
// Compares performance across different order patterns

#include "engine/order.hpp"
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "utils/metrics.hpp"
#include "utils/event_parser.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <string>

using namespace std::chrono;

void benchmark_csv_file(const std::string& filename, const std::string& scenario_name) {
    std::cout << "\n" << scenario_name << " (from: " << filename << ")\n";
    std::cout << std::string(60, '-') << "\n";

    try {
        // Load events from CSV
        auto events = elob::load_events_from_csv(filename);
        
        if (events.empty()) {
            std::cerr << "Warning: No events loaded from " << filename << "\n";
            return;
        }
        
        std::cout << "Loaded " << events.size() << " events\n";

        // Create order book and engine
        elob::OrderBook book;
        elob::MatchingEngine engine(book);
        elob::EventIngestor ingestor(book, engine);

        // Time the ingestion
        auto start = high_resolution_clock::now();
        
        uint64_t total_trades = 0;
        for (auto& event : events) {
            auto trades = ingestor.process(event);
            total_trades += trades.size();
        }
        
        auto end = high_resolution_clock::now();
        auto duration_us = duration_cast<microseconds>(end - start).count();
        auto throughput = (events.size() * 1e6) / duration_us;

        std::cout << "Processed in:        " << std::fixed << std::setprecision(3)
                  << duration_us / 1000.0 << " ms\n";
        std::cout << "Throughput:          " << std::fixed << std::setprecision(0)
                  << throughput << " events/sec\n";
        std::cout << "Trades generated:    " << total_trades << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error processing " << filename << ": " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Event Parser Benchmark ===\n";
    std::cout << "Testing order processing from CSV files\n\n";

    // Benchmark different scenario files
    benchmark_csv_file("data/sample_orders.csv", "Sample Mixed Scenario");
    benchmark_csv_file("data/crossing_orders.csv", "Crossing Scenario");
    benchmark_csv_file("data/spread_orders.csv", "Spread Scenario");

    std::cout << "\n=== Benchmark Complete ===\n";
    return 0;
}
