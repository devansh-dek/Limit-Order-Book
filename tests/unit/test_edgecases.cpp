#include "engine/order_book.hpp"
#include "engine/order.hpp"
#include "engine/matching_engine.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace elob;

static void test_partial_fills_across_levels() {
    OrderBook book;
    Order a1(1, Side::Sell, 100, 3, 1);
    Order a2(2, Side::Sell, 101, 4, 2);
    book.insert(a1);
    book.insert(a2);

    MatchingEngine engine(book);
    std::vector<Trade> trades;

    // taker buy price=101 qty=5 should take 3@100 and 2@101
    Order taker(3, Side::Buy, 101, 5, 10);
    engine.process(taker, 10, trades);

    uint64_t total = 0;
    for (auto& t : trades) total += t.quantity;
    assert(total == 5);
    assert(book.qty_at(Side::Sell, 100) == 0);
    assert(book.qty_at(Side::Sell, 101) == 2);

    std::cout << "partial_fills_across_levels passed\n";
}

static void test_modify_preserve_filled() {
    OrderBook book;
    MatchingEngine engine(book);
    std::vector<Trade> trades;

    Order maker(1, Side::Sell, 100, 10, 1);
    book.insert(maker);

    Order taker(2, Side::Buy, 100, 4, 5);
    engine.process(taker, 5, trades);
    assert(book.qty_at(Side::Sell, 100) == 6);

    // modify to new total qty=8; filled=4, so remaining should be 4
    bool ok = book.modify(1, 100, 8, 10);
    assert(ok);
    assert(book.qty_at(Side::Sell, 100) == 4);

    std::cout << "modify_preserve_filled passed\n";
}

int main() {
    test_partial_fills_across_levels();
    test_modify_preserve_filled();
    std::cout << "EDGECASE TESTS PASSED\n";
    return 0;
}
