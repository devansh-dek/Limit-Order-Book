# Architecture

## Overview

EfficientLimitOrderBook is a deterministic, single-process limit order book matching engine designed for correctness, testability, and performance in that order. It demonstrates price-time priority matching with deterministic replay capabilities.

## Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                        Application                          │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
          ┌─────────────────────┐
          │  EventIngestor      │  ◄── Processes events deterministically
          └──────┬──────────────┘
                 │
         ┌───────┴────────┐
         │                │
         ▼                ▼
  ┌─────────────┐  ┌──────────────┐
  │ OrderBook   │  │ Matching     │  ◄── Price-time priority
  │             │◄─┤ Engine       │      matching logic
  │ - Bids      │  │              │
  │ - Asks      │  └──────────────┘
  │ - Index     │
  └──────┬──────┘
         │
         ▼
  ┌──────────────┐
  │ PriceLevel   │  ◄── FIFO queue at each price
  │ std::list    │
  └──────────────┘
```

## Data Flow

1. **Event Ingestion**: `Event` → `EventIngestor::process()`
2. **Matching**: `EventIngestor` calls `MatchingEngine::process()` for `NewOrder` events
3. **Trade Generation**: Matching produces `Trade` records
4. **Residual Resting**: Unfilled taker orders are inserted into `OrderBook`
5. **Logging**: Optional `Logger` records events and trades to disk

## Determinism Guarantees

- **Algorithmic**: Same event sequence → same trade sequence
- **Data Structures**:
  - `std::map` with explicit comparators (bids descending, asks ascending)
  - `std::list` for stable iterators and FIFO ordering
- **Timestamps**: Logical, caller-provided timestamps (no system clock)
- **Replay**: Events logged to deterministic text format for verification

## Threading Model

- **Single-threaded core**: All components are single-threaded for determinism
- **Optional concurrency**: `EngineMultiThreaded` wraps the engine with a mutex for concurrent access while preserving deterministic event ordering

## Key Design Decisions

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Price containers | `std::map` | Deterministic ordering, log(N) operations |
| Order storage | `std::list<Order>` | Stable iterators, O(1) erase |
| Order index | `std::unordered_map` | O(1) cancel/modify lookup |
| Prices | `int64_t` | Avoid floating-point non-determinism |
| Events | `std::variant` | Type-safe, no dynamic allocation |
| Logging | Text format | Human-readable, easy to debug |

## Performance Characteristics

- **Insert**: O(log P) where P = number of price levels
- **Match**: O(M) where M = number of orders matched
- **Cancel**: O(1) via order index
- **Modify**: O(1) same-price, O(log P) price-change

## Extension Points

- Replace `std::map` with custom skip-lists or flat maps for lower latency
- Add binary logging format for production
- Implement finer-grained locking for multi-symbol concurrency
- Add order types (IOC, FOK, stop-loss) via extended `Event` variants
