// Trade model for EfficientLimitOrderBook
#pragma once

#include <cstdint>

namespace elob {

struct Trade {
    uint64_t trade_id;       // unique trade id (increasing)
    uint64_t maker_order_id; // resting order id
    uint64_t taker_order_id; // incoming order id
    int64_t price;           // execution price
    uint64_t quantity;       // executed quantity
    uint64_t timestamp;      // logical timestamp

    Trade() = default;
    Trade(uint64_t tid, uint64_t maker, uint64_t taker, int64_t p, uint64_t q, uint64_t ts) noexcept
        : trade_id(tid), maker_order_id(maker), taker_order_id(taker), price(p), quantity(q), timestamp(ts) {}
};

} // namespace elob
