#include "engine/order_book.hpp"
#include "engine/order.hpp"

#include <cassert>
#include <iostream>

using namespace elob;

int main() {
    OrderBook book;

    Order o1(1, Side::Buy, 100, 10, 1);
    book.insert(o1);
    auto pl100 = book.find_level(Side::Buy, 100);
    assert(pl100 != nullptr);
    assert(pl100->total_quantity() == 10);

    Order o2(2, Side::Buy, 100, 5, 2);
    book.insert(o2);
    pl100 = book.find_level(Side::Buy, 100);
    assert(pl100 != nullptr);
    assert(pl100->total_quantity() == 15);

    bool modified = book.modify(1, 101, 8, 10);
    assert(modified);

    auto pl101 = book.find_level(Side::Buy, 101);
    assert(pl101 != nullptr);
    assert(pl101->total_quantity() == 8);

    pl100 = book.find_level(Side::Buy, 100);
    // after moving order 1, only order 2 remains at price 100 with qty 5
    assert(pl100 != nullptr && pl100->total_quantity() == 5);

    bool canceled = book.cancel(2);
    assert(canceled);

    pl100 = book.find_level(Side::Buy, 100);
    // price level should be removed or empty
    assert(pl100 == nullptr || pl100->total_quantity() == 0);

    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
