# Efficient Limit Order Book

A **production-grade C++20 matching engine** demonstrating the data structures, algorithms, and infrastructure used in real exchange and HFT systems.

## Features

- **Flat price ladder** — O(1) price-level lookup via `vector<LadderSlot>` indexed by tick offset (replaces `std::map`)
- **Pool-allocated intrusive order list** — zero heap allocation on the hot path; orders live in a pre-allocated `OrderPool`
- **Price-time priority matching** — exchange-accurate FIFO semantics within each price level
- **IOC / FOK order types** — Immediate-or-Cancel discards residual; Fill-or-Kill aborts via pre-match availability check
- **Self-Trade Prevention (STP)** — three modes: CancelNewest, CancelOldest, CancelBoth; keyed on `participant_id`
- **Iceberg orders** — display slice only; reserve replenished to tail after each fill (loses time priority on replenishment)
- **L2 market data publisher** — top-5 bid/ask snapshot with monotone `seq_num` for gap detection
- **Binary write-ahead log (WAL)** — fixed 40-byte packed records, 4 KB batched `write()`, ~2 ns/record amortized
- **rdtsc latency histograms** — TSC-based per-event timing, TSC calibrated against `CLOCK_MONOTONIC`; p50/p99/p99.9 percentiles
- **Pre-trade validator** — 6 reject reasons (SEC Rule 15c3-5 style), O(1) duplicate/unknown-ID checks, monotonic timestamp enforcement
- **SPSC / MPSC concurrency** — lock-free single-producer ring buffer for single-gateway pipelines; mutex-based MPSC for multi-gateway

## Measured Performance (Release Build, x86_64)

Benchmarked with `rdtsc` per-event timing, TSC calibrated to 2.12 GHz.

### Throughput & Latency (N = 50 000 events)

| Scenario | ops/s | p50 | p99 | p99.9 |
|----------|-------|-----|-----|-------|
| **crossing** | **2.05M** | **315 ns** | **1.3 µs** | **2.2 µs** |
| same_price | 1.85M | 431 ns | 1.3 µs | 2.3 µs |
| spread | 1.27M | 549 ns | 3.4 µs | 7.6 µs |

- **crossing**: aggressive orders that always match (fast-path through matching loop)
- **same_price**: alternating buy/sell at one price (high match rate, iceberg-free)
- **spread**: realistic depth-building with a bid-ask spread (more book traversal)

### Throughput by Scale

| Scenario | N = 1 000 | N = 10 000 | N = 50 000 |
|----------|-----------|------------|------------|
| same_price | 2.19M ops/s | 1.22M ops/s | 1.85M ops/s |
| spread | 1.37M ops/s | 0.90M ops/s | 1.27M ops/s |
| crossing | 2.18M ops/s | 2.08M ops/s | 2.05M ops/s |

### WAL Overhead
- Amortized cost: ~2 ns/record (4 096-record batch, single `write()` syscall)
- vs per-event `fsync`: ~20 µs — 10 000× slower

## Architecture

### Core Pipeline

```
                   ┌──────────────┐
 raw event ──────► │  Validator   │ reject → caller (RejectReason)
                   └──────┬───────┘
                           │ pass
                           ▼
                   ┌──────────────┐
                   │EventIngestor │  std::visit dispatch
                   └──┬───────┬───┘
                      │       │
               NewOrder│       │Cancel / Modify
                      ▼       ▼
              ┌──────────────────┐
              │  MatchingEngine  │  IOC / FOK / STP logic
              └────────┬─────────┘
                       │ taker fills / rests
                       ▼
              ┌──────────────────┐
              │    OrderBook     │  flat PriceLadder × 2 (bids, asks)
              │  + order_index_  │  O(1) insert / cancel / modify
              └────────┬─────────┘
                       │
          ┌────────────┴──────────────┐
          │                           │
          ▼                           ▼
  ┌──────────────┐           ┌──────────────────┐
  │  L2Publisher │           │   WalLogger      │
  │ seq-stamped  │           │ 40-byte records  │
  │ top-5 snap   │           │ 4 KB batch write │
  └──────────────┘           └──────────────────┘
```

### Data Structures

| Layer | Structure | Key property |
|-------|-----------|--------------|
| Price index | `vector<LadderSlot>` (flat array) | O(1) lookup by `(price − base) / tick` |
| Order storage | `OrderPool` — flat `PooledOrder[]` | Zero heap alloc on hot path; intrusive prev/next |
| Order queue | Intrusive doubly-linked list in pool | O(1) FIFO insert / erase; no iterator invalidation |
| Order index | `unordered_map<OrderID, Locator>` | O(1) cancel, modify, STP lookup |
| Active-level tracker | `active_count_` + `best_slot_` | Skip O(N) scan when book is empty; O(1) best bid/ask |

### Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Insert limit order | **O(1)** | Flat ladder; pool append |
| Match one level | O(M) | M = orders matched |
| Cancel order | **O(1)** | `order_index_` lookup + intrusive erase |
| Modify (same price) | **O(1)** | In-place qty update |
| Modify (price change) | **O(1)** | Cancel + re-insert (both O(1) on flat ladder) |
| Best bid/ask | **O(1)** | `best_slot_` maintained on every insert/erase |
| FOK pre-check | O(L) | L = levels at-or-better; sums `total_qty` |
| L2 snapshot (top-5) | O(5) = O(1) | Walk from `best_slot_` |

### Order Types

| Type | Behaviour |
|------|-----------|
| Limit | Match up to price; rest residual |
| IOC | Match immediately; discard residual |
| FOK | Pre-check available qty ≥ order qty; match or reject entirely |
| Iceberg | Display `peak_size` only; replenish from reserve after each fill (tail position) |

### STP Modes (`participant_id` match)

| Mode | Effect |
|------|--------|
| `CancelNewest` | Taker is cancelled; maker rests |
| `CancelOldest` | Maker is removed; taker continues |
| `CancelBoth` | Both sides cancelled |

### Concurrency

| Model | Use case | Implementation |
|-------|----------|----------------|
| Single-threaded | Baseline, deterministic replay | No synchronisation |
| Lock-free SPSC | Single gateway → engine pipeline | `LockFreeQueue`: alignas(64) atomic head/tail, power-of-2 ring |
| Mutex MPSC | Multiple gateways → one engine | `EngineMultiThreaded`: `std::mutex` + `lock_guard` |

The engine core is inherently single-threaded (exchange matching is serial). Concurrency models govern how events are routed from producers to that single core.

### Pre-trade Validation

`Validator` sits in front of `EventIngestor` (opt-in; `nullptr` = disabled):

| Reject reason | Condition |
|---------------|-----------|
| `InvalidPrice` | price ≤ 0 |
| `PriceOutOfRange` | price outside `[min_price, max_price]` |
| `InvalidQuantity` | quantity == 0 |
| `DuplicateOrderId` | ID already live in `order_index_` |
| `UnknownOrderId` | Cancel/Modify targets non-existent ID |
| `TimestampNotMonotonic` | `ev.timestamp < last_seen` |

### WAL Record Layout (40 bytes, packed)

```
Offset  Size  Field
0       1     rtype (NewOrder=1, Cancel=2, Modify=3, Trade=4)
1       1     side
2       2     reserved
4       4     seq
8       8     id_a
16      8     id_b
24      8     price
32      8     quantity
```
Writes are batched in a 4 096-record buffer; one `write()` syscall per flush.

## Project Structure

```
src/
├── engine/
│   ├── order.cpp/hpp           — Order model: IOC/FOK/iceberg/STP fields
│   ├── trade.cpp/hpp           — Trade record
│   ├── price_level.cpp/hpp     — Single price-level FIFO queue
│   ├── order_pool.cpp/hpp      — Pool allocator; intrusive PooledOrder
│   ├── price_ladder.cpp/hpp    — Flat vector ladder; best_slot_ tracking
│   ├── order_book.cpp/hpp      — Bid/ask ladders + order_index_
│   └── matching_engine.cpp/hpp — IOC/FOK/STP/iceberg matching
├── data/
│   ├── event.cpp/hpp           — std::variant<NewOrder,Cancel,Modify,Trade>
│   ├── ingestor.cpp/hpp        — Event dispatch; taker resting logic
│   └── validator.cpp/hpp       — Pre-trade validation (6 reject reasons)
├── marketdata/
│   └── l2_publisher.cpp/hpp    — L2Snapshot (seq_num, top-5 bid/ask)
├── logging/
│   ├── logger.cpp/hpp          — Text event/trade log
│   ├── replay.cpp/hpp          — Log replay
│   └── wal_logger.cpp/hpp      — Binary WAL (40-byte packed records)
└── utils/
    ├── metrics.cpp/hpp         — Throughput counters
    ├── latency_histogram.cpp/hpp — rdtsc, TSC calibration, percentiles
    ├── event_parser.hpp        — CSV order file loader
    ├── lockfree_queue.hpp      — SPSC ring buffer
    ├── engine_lockfree.hpp     — Lock-free producer/consumer wrapper
    └── engine_mt.hpp           — Mutex-based multi-producer wrapper

tests/
├── unit/                       — 11 executables (~70 tests total)
│   ├── test_cancel_modify.cpp
│   ├── test_edgecases.cpp
│   ├── test_ingest.cpp
│   ├── test_parser.cpp
│   ├── test_market_orders.cpp  — 10 IOC/FOK tests
│   ├── test_stp.cpp            — 9 STP tests
│   ├── test_iceberg.cpp        — 8 iceberg tests
│   ├── test_l2.cpp             — 8 L2 snapshot tests
│   ├── test_wal.cpp            — 9 WAL tests
│   ├── test_histogram.cpp      — 8 rdtsc histogram tests
│   └── test_validator.cpp      — 16 validation tests
├── concurrency/
│   ├── test_mt.cpp             — Mutex MPSC
│   └── test_lockfree.cpp       — SPSC vs MPSC vs mutex comparison
└── replay/
    └── test_replay.cpp         — Determinism verification

benchmarks/
├── bench_runner.cpp            — rdtsc-timed scenarios → CSV + table
├── test_benchmark.cpp          — Simple throughput baseline
└── bench_parser.cpp            — CSV parser throughput
```

## Quick Start

### Build
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j$(nproc)
```

### Run All Tests
```bash
cd build

./elob_test_cancel         # Cancel / modify operations
./elob_test_edgecases      # Boundary conditions
./elob_test_ingest         # Event processing pipeline
./elob_test_market_orders  # IOC and FOK order types (10 tests)
./elob_test_stp            # Self-trade prevention (9 tests)
./elob_test_iceberg        # Iceberg replenishment (8 tests)
./elob_test_l2             # L2 market data snapshots (8 tests)
./elob_test_wal            # Binary WAL correctness (9 tests)
./elob_test_histogram      # rdtsc histogram percentiles (8 tests)
./elob_test_validator      # Pre-trade validation (16 tests)
./elob_test_concurrency    # Concurrency models comparison
```

### Benchmarks
```bash
./elob_bench_runner   # rdtsc per-event timing → bench_results.csv
./elob_benchmark      # Simple throughput baseline
./elob_bench_parser data/sample_orders.csv
```

## Usage Example

```cpp
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "data/validator.hpp"
#include "marketdata/l2_publisher.hpp"
#include "logging/wal_logger.hpp"

using namespace elob;

// Price ladder: base=1, tick=1, max_price=10000
OrderBook      book(1, 1, 10000);
MatchingEngine engine(book);
Validator      validator(book, 1, 10000);
EventIngestor  ingestor(book, engine, &validator);  // null = no validation

WalLogger      wal("orders.wal");
L2Publisher    l2pub(book);

// Limit buy: id=1, side=Buy, price=99, qty=100, ts=1
Order buy(1, Side::Buy, 99, 100, 1);
Event e1(1, 1, NewOrder{buy});
auto trades = ingestor.process(e1);  // rests (no match)
wal.push(e1);

// IOC sell: crosses at 99, discards residual
Order ioc_sell(2, Side::Sell, 99, 200, 2, OrderType::IOC);
Event e2(2, 2, NewOrder{ioc_sell});
trades = ingestor.process(e2);       // Trade(price=99, qty=100)

// Iceberg buy: 10 displayed, 90 in reserve
Order ice(3, Side::Buy, 98, 100, 3);
ice.set_iceberg(10);
Event e3(3, 3, NewOrder{ice});
ingestor.process(e3);

// L2 snapshot after each event
L2Snapshot snap = l2pub.build(3);
// snap.seq_num == 1, snap.bids[0] = {price=98, qty=10}

// Cancel
Event e4(4, 4, Cancel{1});
ingestor.process(e4);
wal.push(e4);
wal.flush();
```

## Why This Demonstrates Quant/HFT Readiness

### Market Microstructure
- Exchange-accurate price-time priority (FIFO within level)
- All standard order types: Limit, IOC, FOK, Iceberg
- STP as mandated by exchange rules (CME, CBOE)
- L2 market data with gap-detectable sequence numbers

### Low-Latency Engineering
- **O(1) everything on the hot path**: insert, cancel, modify, best bid/ask
- Zero heap allocation during matching (pool allocator)
- Cache-line aware: `OrderPool` entries packed; SPSC atomics `alignas(64)`
- `rdtsc` directly (not `std::chrono`) for sub-nanosecond measurement resolution
- TSC calibrated once at startup; no per-event syscalls

### Risk & Compliance Infrastructure
- Pre-trade validator (SEC Rule 15c3-5 pattern): duplicate IDs, price bands, monotonic timestamps
- Binary WAL with sequence numbers: crash-recoverable, replayable audit trail
- Deterministic replay: same event log → identical trade sequence (100% reproducible)

### Engineering Craft
- Zero external dependencies (C++20 stdlib only)
- `std::visit` on `std::variant` — type-safe event dispatch, no virtual dispatch overhead
- Integer prices throughout — no floating-point non-determinism
- 70+ unit tests across 11 executables; every new subsystem ships with a test file

## Technical Requirements

- **Compiler**: C++20 (GCC 10+, Clang 12+)
- **Build system**: CMake 3.15+
- **Threading**: pthread (concurrency tests)

## Author

Devansh Khandelwal
