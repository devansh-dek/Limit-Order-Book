#include "elob/matching_engine.hpp"
#include "elob/price_level.hpp"

#include <algorithm>

namespace elob {

static inline uint64_t min_u64(uint64_t a, uint64_t b) noexcept { return (a < b) ? a : b; }

void MatchingEngine::match_buy(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades) {
    while (!taker.is_filled()) {
        PriceLevel *pl = book_.best_ask();
        if (!pl) break;
        if (pl->price() > taker.price) break; // best ask too expensive

        // iterate makers at this price level
        auto &orders = pl->orders();
        for (auto it = pl->begin(); it != pl->end() && !taker.is_filled(); ) {
            Order &maker = *it;
            uint64_t qty = min_u64(taker.remaining, maker.remaining);
            maker.fill(qty);
            taker.fill(qty);

            out_trades.emplace_back(next_trade_id_++, maker.order_id, taker.order_id, maker.price, qty, timestamp);

            if (maker.is_filled()) {
                it = pl->erase_order(it);
            } else {
                ++it;
            }
        }

        // if price level empty, remove it
        if (pl->empty()) book_.remove_level_if_empty(Side::Sell, pl->price());
    }
}

void MatchingEngine::match_sell(Order &taker, uint64_t timestamp, std::vector<Trade> &out_trades) {
    while (!taker.is_filled()) {
        PriceLevel *pl = book_.best_bid();
        if (!pl) break;
        if (pl->price() < taker.price) break; // best bid too low

        auto &orders = pl->orders();
        for (auto it = pl->begin(); it != pl->end() && !taker.is_filled(); ) {
            Order &maker = *it;
            uint64_t qty = min_u64(taker.remaining, maker.remaining);
            maker.fill(qty);
            taker.fill(qty);

            out_trades.emplace_back(next_trade_id_++, maker.order_id, taker.order_id, maker.price, qty, timestamp);

            if (maker.is_filled()) {
                it = pl->erase_order(it);
            } else {
                ++it;
            }
        }

        if (pl->empty()) book_.remove_level_if_empty(Side::Buy, pl->price());
    }
}

} // namespace elob
