# API Reference

## Core Domain Models

### `Order`
```cpp
struct Order {
    uint64_t order_id;
    Side side;               // Buy or Sell
    int64_t price;
    uint64_t quantity;
    uint64_t remaining;
    uint64_t timestamp;

    uint64_t fill(uint64_t n) noexcept;
    bool is_filled() const noexcept;
};
```

### `Trade`
```cpp
struct Trade {
    uint64_t trade_id;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    int64_t price;
    uint64_t quantity;
    uint64_t timestamp;
};
```

### `Event`
```cpp
struct NewOrder { Order order; };
struct Cancel { uint64_t order_id; };
struct Modify { uint64_t order_id; uint64_t new_quantity; int64_t new_price; };

using EventPayload = std::variant<std::monostate, NewOrder, Cancel, Modify, Trade>;

struct Event {
    uint64_t event_id;
    uint64_t timestamp;
    EventPayload payload;
};
```

## OrderBook

```cpp
class OrderBook {
public:
    void insert(const Order& o);
    bool cancel(uint64_t order_id);
    bool modify(uint64_t order_id, Price new_price, uint64_t new_quantity, uint64_t new_timestamp);

    PriceLevel* find_level(Side side, Price price);
    PriceLevel* best_bid();
    PriceLevel* best_ask();
    void remove_level_if_empty(Side side, Price price);
};
```

## MatchingEngine

```cpp
class MatchingEngine {
public:
    explicit MatchingEngine(OrderBook &book) noexcept;

    // Process taker order, emit trades
    void process(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades);

    uint64_t next_trade_id() const noexcept;
};
```

## EventIngestor

```cpp
class EventIngestor {
public:
    EventIngestor(OrderBook &book, MatchingEngine &engine) noexcept;

    // Process event and return emitted trades
    std::vector<Trade> process(Event &ev);
};
```

## Logger

```cpp
class Logger {
public:
    explicit Logger(const std::string &path);

    void log_event(const Event &ev);
    void log_trade(const Trade &t);
};
```

## EngineMultiThreaded

```cpp
class EngineMultiThreaded {
public:
    EngineMultiThreaded();

    // Thread-safe event processing
    std::vector<Trade> process_event(Event &ev);
};
```

## Usage Examples

### Basic Matching
```cpp
#include "elob/order_book.hpp"
#include "elob/matching_engine.hpp"

elob::OrderBook book;
elob::MatchingEngine engine(book);

// Insert resting sell order
elob::Order sell(1, elob::Side::Sell, 100, 10, 1);
book.insert(sell);

// Match with buy taker
elob::Order buy(2, elob::Side::Buy, 100, 5, 2);
std::vector<elob::Trade> trades;
engine.process(buy, 2, trades);

// trades now contains 1 trade: qty=5 at price=100
```

### Event Ingestion
```cpp
#include "elob/ingestor.hpp"

elob::OrderBook book;
elob::MatchingEngine engine(book);
elob::EventIngestor ingestor(book, engine);

elob::Order o(1, elob::Side::Buy, 100, 10, 1);
elob::NewOrder no{o};
elob::Event ev(1, 1, elob::EventPayload(no));

auto trades = ingestor.process(ev);
```

### Logging & Replay
```cpp
#include "elob/logger.hpp"

elob::Logger logger("trades.log");

// During normal operation
logger.log_event(ev);
for (auto &t : trades) logger.log_trade(t);

// Replay verification
// Run: ./replay trades.log
```

### Cancel & Modify
```cpp
// Cancel
bool ok = book.cancel(order_id);

// Modify (preserves filled quantity)
bool modified = book.modify(order_id, new_price, new_quantity, new_timestamp);
```

### Multi-threaded
```cpp
#include "elob/engine_mt.hpp"

elob::EngineMultiThreaded engine;

// From multiple threads:
auto trades = engine.process_event(event);
```
