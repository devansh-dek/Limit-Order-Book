// Matching engine: price-time priority matching with partial fills
#pragma once

#include "engine/order_book.hpp"
#include "engine/trade.hpp"

#include <vector>
#include <cstdint>

namespace elob {

class MatchingEngine {
public:
    explicit MatchingEngine(OrderBook &book) noexcept : book_(book), next_trade_id_(1) {}

    // Process an incoming (taker) order, produce trades. The taker is modified (remaining decreases).
    // IOC residual: caller should discard (not insert into book).
    // FOK: if insufficient liquidity, returns immediately with no trades and taker unchanged.
    void process(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades) {
        if (taker.type == OrderType::FOK) {
            if (book_.available_to_fill(taker.side, taker.price) < taker.quantity)
                return;  // cancel entire order — do not touch the book
        }
        if (taker.side == Side::Buy) match_buy(taker, timestamp, out_trades);
        else                          match_sell(taker, timestamp, out_trades);
    }

    uint64_t next_trade_id() const noexcept { return next_trade_id_; }

private:
    void match_buy(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades);
    void match_sell(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades);

    OrderBook &book_;
    uint64_t next_trade_id_;
};

} // namespace elob
