#include "data/ingestor.hpp"
#include "engine/order.hpp"
#include "engine/matching_engine.hpp"

#include <cassert>
#include <iostream>

using namespace elob;

int main() {
    OrderBook book;
    MatchingEngine engine(book);
    EventIngestor ingestor(book, engine);

    // resting sell at price 100 qty 5
    Order sell(1, Side::Sell, 100, 5, 1);
    book.insert(sell);

    // incoming buy taker price 100 qty 3
    Order buy(2, Side::Buy, 100, 3, 10);
    NewOrder no{buy};
    Event ev(1, 10, EventPayload(no));

    auto trades = ingestor.process(ev);
    assert(!trades.empty());
    assert(trades[0].quantity == 3);
    assert(trades[0].maker_order_id == 1);
    assert(trades[0].taker_order_id == 2);

    // maker should have 2 remaining
    assert(book.qty_at(Side::Sell, 100) == 2);

    std::cout << "INGEST TEST PASSED\n";
    return 0;
}
