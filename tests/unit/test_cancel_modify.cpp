#include "engine/order_book.hpp"
#include "engine/order.hpp"

#include <cassert>
#include <iostream>

using namespace elob;

int main() {
    OrderBook book;

    Order o1(1, Side::Buy, 100, 10, 1);
    book.insert(o1);
    assert(book.qty_at(Side::Buy, 100) == 10);

    Order o2(2, Side::Buy, 100, 5, 2);
    book.insert(o2);
    assert(book.qty_at(Side::Buy, 100) == 15);

    bool modified = book.modify(1, 101, 8, 10);
    assert(modified);
    assert(book.qty_at(Side::Buy, 101) == 8);
    assert(book.qty_at(Side::Buy, 100) == 5);

    bool canceled = book.cancel(2);
    assert(canceled);
    assert(book.qty_at(Side::Buy, 100) == 0);

    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
