// PriceLevel: holds orders at a single price and preserves time-priority
#pragma once

#include "elob/order.hpp"

#include <list>
#include <cstdint>
#include <numeric>

namespace elob {

class PriceLevel {
public:
    using OrderList = std::list<Order>;
    using OrderIterator = OrderList::iterator;

    explicit PriceLevel(int64_t p) noexcept : price_(p) {}

    // Add an order to the tail (time-priority). Returns iterator to the stored order.
    OrderIterator add_order(const Order& o) {
        orders_.push_back(o);
        return std::prev(orders_.end());
    }

    // Erase order by iterator. Returns next iterator.
    OrderIterator erase_order(OrderIterator it) {
        return orders_.erase(it);
    }

    uint64_t total_quantity() const noexcept {
        uint64_t total = 0;
        for (const auto &o : orders_) total += o.remaining;
        return total;
    }

    bool empty() const noexcept { return orders_.empty(); }
    int64_t price() const noexcept { return price_; }
    const OrderList& orders() const noexcept { return orders_; }

private:
    int64_t price_;
    OrderList orders_;
};

} // namespace elob
