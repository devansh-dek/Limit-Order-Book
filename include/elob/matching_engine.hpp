// Matching engine: price-time priority matching with partial fills
#pragma once

#include "elob/order_book.hpp"
#include "elob/trade.hpp"

#include <vector>
#include <cstdint>

namespace elob {

class MatchingEngine {
public:
    explicit MatchingEngine(OrderBook &book) noexcept : book_(book), next_trade_id_(1) {}

    // Process an incoming (taker) order, produce trades. The taker is modified (remaining decreases).
    // The caller may insert the leftover taker order into the book if desired.
    void process(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades) {
        if (taker.side == Side::Buy) {
            match_buy(taker, timestamp, out_trades);
        } else {
            match_sell(taker, timestamp, out_trades);
        }
    }

    uint64_t next_trade_id() const noexcept { return next_trade_id_; }

private:
    void match_buy(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades);
    void match_sell(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades);

    OrderBook &book_;
    uint64_t next_trade_id_;
};

} // namespace elob
