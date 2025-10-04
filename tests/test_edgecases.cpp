#include "elob/order_book.hpp"
#include "elob/order.hpp"
#include "elob/matching_engine.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace elob;

static void test_partial_fills_across_levels() {
    OrderBook book;
    // resting asks: id1 price100 qty3, id2 price101 qty4
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
    for (auto &t : trades) total += t.quantity;
    assert(total == 5);
    // verify quantities at price levels
    auto pl100 = book.find_level(Side::Sell, 100);
    assert(pl100 == nullptr || pl100->total_quantity() == 0);
    auto pl101 = book.find_level(Side::Sell, 101);
    assert(pl101 != nullptr && pl101->total_quantity() == 2);

    std::cout << "partial_fills_across_levels passed\n";
}

static void test_modify_preserve_filled() {
    OrderBook book;
    MatchingEngine engine(book);
    std::vector<Trade> trades;

    // resting sell order id1 qty=10
    Order maker(1, Side::Sell, 100, 10, 1);
    book.insert(maker);

    // taker buys 4
    Order taker(2, Side::Buy, 100, 4, 5);
    engine.process(taker, 5, trades);

    // maker should have remaining 6
    auto pl = book.find_level(Side::Sell, 100);
    assert(pl != nullptr && pl->total_quantity() == 6);

    // modify maker to new total quantity 8 (should preserve 4 filled => remaining = 8-4=4)
    bool ok = book.modify(1, 100, 8, 10);
    assert(ok);
    pl = book.find_level(Side::Sell, 100);
    assert(pl != nullptr && pl->total_quantity() == 4);

    std::cout << "modify_preserve_filled passed\n";
}

int main() {
    test_partial_fills_across_levels();
    test_modify_preserve_filled();
    std::cout << "EDGECASE TESTS PASSED\n";
    return 0;
}
