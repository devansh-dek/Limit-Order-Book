# Phase 15: Event Parser

## Overview
Phase 15 implements a CSV event parser for realistic order input replay. This enables deterministic testing with different market scenarios and provides a foundation for realistic benchmark workloads.

## Key Features

### Event Parser (`src/utils/event_parser.hpp`)
- **CSV Parsing**: Reads order events from CSV files
- **Format Support**:
  - NEW ORDERS: `timestamp,order_id,side,price,quantity`
  - CANCEL: `CANCEL,order_id`
  - MODIFY: `MODIFY,order_id,new_price,new_quantity`
- **Error Handling**: Reports line numbers and detailed error messages
- **Comment Support**: Lines starting with `#` are skipped
- **Whitespace Trimming**: Automatic whitespace handling

### Sample Data Files
Located in `data/`:
- `sample_orders.csv` - Mixed scenario with orders, cancels, and modifies
- `crossing_orders.csv` - High-crossing scenario (many matching trades)
- `spread_orders.csv` - Bid-ask spread scenario with depth

### Test & Benchmark Utilities
- `tests/unit/test_parser.cpp`: Parser demo - loads CSV and displays parsed events
- `benchmarks/bench_parser.cpp`: Performance benchmark across scenarios

## Usage

### Parse Events from CSV
```cpp
#include "utils/event_parser.hpp"

auto events = elob::load_events_from_csv("data/sample_orders.csv");

// Process events with EventIngestor
elob::OrderBook book;
elob::MatchingEngine engine(book);
elob::EventIngestor ingestor(book, engine);

for (auto& event : events) {
    auto trades = ingestor.process(event);
    // Handle trades...
}
```

### Run Parser Demo
```bash
./build/elob_test_parser data/sample_orders.csv
```

### Run Parser Benchmark
```bash
./build/elob_bench_parser
```

## Performance Results (Phase 15)

Throughput on sample scenarios:
- Sample Mixed: ~392K events/sec
- Crossing: ~684K events/sec
- Spread: ~500K events/sec

## File Organization

```
src/utils/event_parser.hpp          - CSV parser implementation
data/sample_orders.csv              - Sample order file
data/crossing_orders.csv            - Crossing scenario
data/spread_orders.csv              - Spread scenario
tests/unit/test_parser.cpp          - Parser demo executable
benchmarks/bench_parser.cpp         - Parser benchmark
```

## Future Enhancements
- Support for additional input formats (JSON, protobuf)
- Time-series replay with realistic timing
- Integration with Phase 13 visualization for CSV-based benchmarks
- Support for order modifications during replay
