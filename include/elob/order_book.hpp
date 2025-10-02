// OrderBook: maintain bid and ask price levels (no matching yet)
#pragma once

#include "elob/price_level.hpp"
#include "elob/order.hpp"

#include <map>
#include <memory>
#include <cstdint>

namespace elob {

class OrderBook {
public:
    using Price = int64_t;

    OrderBook() = default;

    // Insert order into book; returns iterator/handles could be added later.
    void insert(const Order& o) {
        auto &map_ref = (o.side == Side::Buy) ? bids_ : asks_;
        auto it = map_ref.find(o.price);
        if (it == map_ref.end()) {
            auto pl = std::make_unique<PriceLevel>(o.price);
            pl->add_order(o);
            map_ref.emplace(o.price, std::move(pl));
        } else {
            it->second->add_order(o);
        }
    }

    // Find price level for side/price
    PriceLevel* find_level(Side side, Price price) {
        auto &map_ref = (side == Side::Buy) ? bids_ : asks_;
        auto it = map_ref.find(price);
        return (it == map_ref.end()) ? nullptr : it->second.get();
    }

private:
    // Bids: map keyed by price descending (highest bid first)
    std::map<Price, std::unique_ptr<PriceLevel>, std::greater<Price>> bids_;
    // Asks: map keyed by price ascending (lowest ask first)
    std::map<Price, std::unique_ptr<PriceLevel>, std::less<Price>> asks_;
};

} // namespace elob
