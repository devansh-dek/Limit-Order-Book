// OrderBook: maintain bid and ask price levels (no matching yet)
#pragma once

#include "engine/price_level.hpp"
#include "engine/order.hpp"

#include <map>
#include <memory>
#include <cstdint>
#include <unordered_map>

namespace elob {

class OrderBook {
public:
    using Price = int64_t;

    OrderBook() = default;

    // Insert order into book; returns iterator/handles could be added later.
    void insert(const Order& o) {
        if (o.side == Side::Buy) {
            auto it = bids_.find(o.price);
            PriceLevel *pl = nullptr;
            if (it == bids_.end()) {
                auto up = std::make_unique<PriceLevel>(o.price);
                pl = up.get();
                auto res = bids_.emplace(o.price, std::move(up));
                it = res.first;
            } else {
                pl = it->second.get();
            }
            auto oit = pl->add_order(o);
            order_index_.emplace(o.order_id, Locator{o.side, o.price, pl, oit, o.timestamp});
        } else {
            auto it = asks_.find(o.price);
            PriceLevel *pl = nullptr;
            if (it == asks_.end()) {
                auto up = std::make_unique<PriceLevel>(o.price);
                pl = up.get();
                auto res = asks_.emplace(o.price, std::move(up));
                it = res.first;
            } else {
                pl = it->second.get();
            }
            auto oit = pl->add_order(o);
            order_index_.emplace(o.order_id, Locator{o.side, o.price, pl, oit, o.timestamp});
        }
    }

    // Find price level for side/price
    PriceLevel* find_level(Side side, Price price) {
        if (side == Side::Buy) {
            auto it = bids_.find(price);
            return (it == bids_.end()) ? nullptr : it->second.get();
        } else {
            auto it = asks_.find(price);
            return (it == asks_.end()) ? nullptr : it->second.get();
        }
    }

    // Best price accessors (nullptr if none)
    PriceLevel* best_bid() {
        if (bids_.empty()) return nullptr;
        return bids_.begin()->second.get();
    }

    PriceLevel* best_ask() {
        if (asks_.empty()) return nullptr;
        return asks_.begin()->second.get();
    }

    // Remove a price level if it's empty
    void remove_level_if_empty(Side side, Price price) {
        if (side == Side::Buy) {
            auto it = bids_.find(price);
            if (it != bids_.end() && it->second->empty()) bids_.erase(it);
        } else {
            auto it = asks_.find(price);
            if (it != asks_.end() && it->second->empty()) asks_.erase(it);
        }
    }

    // Cancel an order by id. Returns true if removed.
    bool cancel(uint64_t order_id);

    // Modify an order: change price and/or quantity and optionally timestamp.
    // Returns true on success.
    bool modify(uint64_t order_id, Price new_price, uint64_t new_quantity, uint64_t new_timestamp);

private:
    // Bids: map keyed by price descending (highest bid first)
    std::map<Price, std::unique_ptr<PriceLevel>, std::greater<Price>> bids_;
    // Asks: map keyed by price ascending (lowest ask first)
    std::map<Price, std::unique_ptr<PriceLevel>, std::less<Price>> asks_;

    using OrderIterator = PriceLevel::OrderIterator;

    struct Locator {
        Side side;
        Price price;
        PriceLevel* level;
        OrderIterator it;
        uint64_t timestamp;
    };

    std::unordered_map<uint64_t, Locator> order_index_;
};

} // namespace elob
