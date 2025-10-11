# EfficientLimitOrderBook — Guide

This guide explains everything you need to understand this C++ project, how it's built with CMake, what an electronic limit order book (ELOB) is, and how this repository implements it. It's written for someone familiar with competitive programming C++ but new to production C++ and CMake.

---

## Who this is for

- You know C++ (competitive programming style).
- You don't know CMake, build systems, or production architecture.
- You want to understand this repo end-to-end: files, flow, data structures, build and run.

---

**Quick repo overview**

- Project root: this repository implements an efficient limit order book and test/benchmark harness.
- Important folders/files (quick):
  - `include/` : public headers (core types & interfaces)
  - `src/` : implementation (.cpp files)
  - `tests/` : unit / integration tests
  - `build/` : CMake build outputs and generated binaries
  - `CMakeLists.txt` : top-level build configuration

When you want to edit or read code, the main logical modules live in `include/elob/` and `src/`.

---

**High-level purpose**

This code implements an in-memory limit order book (LOB) and a matching engine that ingests orders, matches them according to price-time priority, emits trades, and provides utilities for ingest/replay/benchmarks. It focuses on performance and correctness.

---

## Part 1 — What is CMake and how to build this repo

### What is CMake

CMake is a cross-platform build-system generator. Instead of specifying compiler commands directly, you write `CMakeLists.txt` files describing targets and dependencies, and CMake produces native build files (Makefiles, Ninja files, Visual Studio projects). Typical workflow:

1. Create a separate `build/` directory.
2. Run `cmake` from `build/` pointing to the project root.
3. Run the generated build command (e.g., `make` or `cmake --build .`).

This repo follows that pattern.

### Build commands (copy/paste)

```
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -- -j
```

- `-DCMAKE_BUILD_TYPE=Debug` builds with debug symbols (use `Release` for optimized builds).
- After building you'll find binaries under `build/` (e.g., test/bench executables).

### Running tests / example

From `build/` after building:

```
./elob_test_ingest
./elob_test_mt
./elob_test_replay
```

Replace names with the executables present in your `build` directory.

---

## Part 2 — Basics of an Order Book (financial primer)

An electronic limit order book (ELOB) stores limit orders (instructions to buy or sell at a specified price or better) and matches them to produce trades.

Key concepts:
- Order: (`id`, side, price, size, timestamp)
- Side: `Bid` (buy) or `Ask` (sell)
- Best bid: highest buy price. Best ask: lowest sell price.
- Price-time priority: when matching, better price first; among same price, earlier time first.

Example — simple book state (price:size):

Asks (sell):

  101.00 : 200
  102.00 : 150
  103.50 : 100

Bids (buy):

  100.50 : 300
  100.00 : 50

Best ask = 101.00 (lowest ask)
Best bid = 100.50 (highest bid)

If a market buy order for 250 arrives, it will consume asks starting at 101.00 (200) then 102.00 (50 of 150), resulting in trades and leaving the rest of the 102.00 level with 100 remaining.

Sequence diagram (simplified ASCII):

Incoming BUY 250 @ MKT
  |
  v
Match 200 @ 101.00 -> Trade(200)
Remaining BUY 50
Match 50 @ 102.00 -> Trade(50)

Result: asks at 101.00 removed, asks at 102.00 reduced to 100.

---

## Part 3 — How this repo models the order book (code mapping)

I'll map core concepts to the files in this repo. When I mention a file, it's the path under the repo root.

- `include/elob/order.hpp` and `src/order.cpp` — `Order` data structure: id, side, price, size, timestamps and helper methods.
- `include/elob/price_level.hpp` and `src/price_level.cpp` — `PriceLevel` structure: holds a queue of `Order`s at the same price (maintains time priority) and aggregated size.
- `include/elob/order_book.hpp` and `src/order_book.cpp` — `OrderBook` class: two side containers (bids and asks), operations for insert/cancel/modify and functions to query levels.
- `include/elob/matching_engine.hpp` and `src/matching_engine.cpp` — `MatchingEngine` (or the top-level engine): receives incoming orders, runs matching algorithm against the `OrderBook`, generates `Trade` objects, and calls hooks (logging, metrics).
- `include/elob/trade.hpp` and `src/trade.cpp` — `Trade` object: a matched quantity at a price between aggressor and passive order.
- `include/elob/ingestor.hpp` and `src/ingestor.cpp` — Order ingestion / parsing of input streams or test events. Useful for benchmarks and replays.
- `include/elob/logger.hpp` and `src/logger.cpp` — Logging utilities.
- `include/elob/metrics.hpp` and `src/metrics.cpp` — Collection of metrics and counters (latency, throughput, event counts).
- `src/replay.cpp` — replay utilities to replay a captured event stream into the engine.
- `src/main.cpp` — lightweight runner and example hooking pieces together (if present).

Files for tests and benchmarks live under `tests/` and generate the `elob_test_*` and `elob_benchmark` binaries.

---

## Part 4 — Data structures and algorithms used here

This code focuses on low-latency operations. Typical design choices in such projects are:

- Price levels stored in an ordered container (e.g., `std::map`, `std::multimap`, or custom tree) keyed by price for quick access to best prices.
- At each price, orders are kept in a FIFO structure (usually linked list or deque) to enforce time-priority.
- A hash map from `order_id` to a pointer/iterator into the price-level queue to support O(1) cancel/modify.

Typical operations and complexity:
- Insert limit order: find price level (log N), append to level queue (O(1)), add id->entry mapping (O(1)).
- Cancel order: lookup by id (O(1)), remove from price-level queue (O(1)), update aggregated size.
- Match (incoming aggressor): repeatedly check best opposing price and consume price-levels until aggressor size is filled or no match possible.

This repo implements these patterns in `order_book.cpp` and `price_level.cpp`.

---

## Part 5 — Matching flow with a concrete example (numbers)

Initial book state (levels shown best-first):

Asks:
 - 101.00 : 200 (orders: A@101:100 @ t1, B@101:100 @ t2)
 - 102.00 : 150 (order C@102:150)

Bids:
 - 100.50 : 300 (order D@100.5:300)

Event: New incoming limit BUY 250 @ 102.50 (aggressor)

Matching algorithm (price-time priority):
1. Compare incoming buy price (102.50) to best ask (101.00).
2. Since 102.50 >= 101.00, match at existing ask prices (resting orders get their price).
3. Consume 200 @ 101.00 (first trade: 200 at 101.00). Remaining incoming size: 50.
4. Next best ask: 102.00 with 150 available. Consume 50 @ 102.00 (second trade: 50 at 102.00). Incoming exhausted.

Resulting trades:
- Trade1: 200 @ 101.00 (aggressor buys 200)
- Trade2: 50 @ 102.00

State after:
- Asks: 102.00 now has 100 remaining (150 - 50)
- Bids unchanged

Notes: the trade price is taken from the resting order price (passive side) by typical LOB rules, though systems may differ on exact conventions (mid-point, taker price, etc.). This repository follows the usual convention: traded price equals the resting order price.

---

## Part 6 — Sequence of events inside this codebase

1. `Ingest` (via `ingestor`) receives an event (limit order, cancel, modify, market order).
2. The `MatchingEngine` validates the event and transforms it into internal `Order` objects.
3. If the event is an aggressive order (crosses the spread), engine calls `OrderBook::match(...)`:
   - Repeat: query best opposing `PriceLevel`
   - While aggressor.size > 0 and best price is matchable:
     - Pop or partially consume orders from the price level FIFO (time priority)
     - Create `Trade` objects and forward to logging/metrics
   - If aggressor still has remaining size and is a limit order, insert remaining quantity into `OrderBook` resting at its price.
4. If the event is cancel/modify: engine locates order by id (map) and updates/removes it.
5. Logging/metrics subsystems record the trades and state snapshots.

File-level walk (where to look for each step):
- Ingest/parsing: `src/ingestor.cpp`, `include/elob/ingestor.hpp`
- Matching engine entry points: `include/elob/matching_engine.hpp` / `src/matching_engine.cpp`
- Book operations: `include/elob/order_book.hpp` / `src/order_book.cpp`
- Level bookkeeping: `include/elob/price_level.hpp` / `src/price_level.cpp`
- Orders/trades: `include/elob/order.hpp`, `include/elob/trade.hpp`

---

## Part 7 — How to read the code (recommended order)

1. `include/elob/order.hpp` — see how an `Order` is represented.
2. `include/elob/price_level.hpp` — understand queueing at a single price.
3. `include/elob/order_book.hpp` — see the container layout for both sides.
4. `include/elob/matching_engine.hpp` — entry point that uses `OrderBook`.
5. `src/ingestor.cpp` & tests — see example inputs and how events enter the system.

Start from headers (clean interface) then follow to `.cpp` for implementation details.

---

## Part 8 — Debugging tips & common tasks

- If a test fails, run the specific test binary in `build/` directly for focused output.
- Add logging in `src/logger.cpp` or use breakpoints in the matching loop inside `src/matching_engine.cpp`.
- For performance work, use `Release` builds and run the benchmarking harness `elob_benchmark`.

Useful build commands:

```
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j
./elob_benchmark
```

---

## Part 9 — Example: Add a limit order (step-by-step code path)

Suppose a new limit buy order arrives in `ingestor`.

1. `ingestor` parses input -> creates an `Order` object and calls `MatchingEngine::handleOrder(order)`.
2. `MatchingEngine` checks if order price crosses best ask (ask book examined via `OrderBook::bestAsk()`).
3. If it crosses, matching loop executes in `OrderBook::matchBuy(order)` or a similar method in `matching_engine.cpp` which:
   - Retrieves the best price level and pops passive `Order`s (first-in) until the incoming order is satisfied or no matchable price levels remain.
   - Produces `Trade` objects and records them via `logger` and `metrics`.
4. If order still has remaining quantity, `OrderBook::insert(order)` is called to add it to the bid side as a resting order.

This repo separates concerns: the `MatchingEngine` orchestrates; `OrderBook` mutates state; `PriceLevel` manages per-price queues.

---

## Part 10 — Contributing and making changes safely

- When changing matching logic, update and run tests under `tests/`.
- Use `Debug` builds while developing; test with sanitizers if available.
- Keep public API stable in `include/elob/*.hpp` where possible.

---

## Quick reference — important files (paths)

- `include/elob/order.hpp`
- `include/elob/price_level.hpp`
- `include/elob/order_book.hpp`
- `include/elob/matching_engine.hpp`
- `include/elob/trade.hpp`
- `src/order.cpp`
- `src/price_level.cpp`
- `src/order_book.cpp`
- `src/matching_engine.cpp`
- `src/ingestor.cpp`
- `src/logger.cpp`
- `src/metrics.cpp`
- `src/replay.cpp`

Open these headers first to get a mental model, then read implementations.

---

## Appendix — quick glossary

- Aggressor: the incoming order that initiates matching.
- Passive / resting: orders already in the book.
- Fill: a trade that consumes some or all of an order's quantity.
- Cancel: removal of a resting order by id.
- Modify: change size/price of an existing order (implemented as cancel+insert in many systems).

---

If you'd like, I can:
- Link code sections to the exact lines in files for a guided walkthrough,
- Add small UML/sequence diagrams (PNG/SVG), or
- Walk through a live example by running a replay with a sample event file.

Tell me which of the above you'd like next.
