#pragma once

#include "data/event.hpp"
#include "engine/order_book.hpp"

#include <cstdint>

namespace elob {

// Reason an event was rejected before touching the book.
// Mirrors SEC Rule 15c3-5 (Market Access Rule) pre-trade risk checks.
enum class RejectReason : uint8_t {
    None                  = 0,
    InvalidPrice,           // price <= 0
    InvalidQuantity,        // quantity == 0
    DuplicateOrderId,       // order_id already resting in the book
    UnknownOrderId,         // cancel/modify references an order not in the book
    PriceOutOfRange,        // price outside [min_price_, max_price_]
    TimestampNotMonotonic,  // timestamp < last seen timestamp
};

const char* reject_reason_str(RejectReason r) noexcept;

// Stateful pre-trade validator. Must be called before EventIngestor::process().
// Holds a reference to the live OrderBook for order-existence checks.
class Validator {
public:
    Validator(const OrderBook& book,
              int64_t min_price, int64_t max_price) noexcept
        : book_(book), min_price_(min_price), max_price_(max_price) {}

    // Returns RejectReason::None if the event may proceed; otherwise the reason.
    // Updates internal state (last_timestamp_) on every call.
    RejectReason validate(const Event& ev);

    uint64_t reject_count() const noexcept { return reject_count_; }
    void     reset_stats()        noexcept { reject_count_ = 0; last_timestamp_ = 0; }

private:
    RejectReason reject(RejectReason r) noexcept {
        ++reject_count_;
        return r;
    }

    const OrderBook& book_;
    int64_t  min_price_;
    int64_t  max_price_;
    uint64_t last_timestamp_ = 0;
    uint64_t reject_count_   = 0;
};

} // namespace elob
