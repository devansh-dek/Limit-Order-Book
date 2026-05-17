// Order model for EfficientLimitOrderBook
#pragma once

#include <cstdint>
#include <optional>

namespace elob {

enum class Side      : uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : uint8_t { Limit = 0, IOC = 1, FOK = 2 };

// Self-trade prevention mode applied to the incoming (taker) order.
// CancelNewest : cancel the taker; resting maker is untouched.
// CancelOldest : cancel the resting maker; taker continues matching.
// CancelBoth   : cancel both; no fill, taker not rested on book.
enum class STPMode   : uint8_t { None = 0, CancelNewest = 1, CancelOldest = 2, CancelBoth = 3 };

struct Order {
    uint64_t  order_id;                        // unique identifier
    Side      side;                            // buy or sell
    int64_t   price;                           // integer price (use integer to avoid FP)
    uint64_t  quantity;                        // original quantity
    uint64_t  remaining;                       // displayed remaining (current slice)
    uint64_t  timestamp;                       // monotonic logical timestamp for time-priority
    uint64_t  participant_id  = 0;             // firm / trader identifier for STP
    uint64_t  peak_size       = 0;             // iceberg: max display qty per slice; 0 = plain order
    uint64_t  reserve_qty     = 0;             // iceberg: remaining hidden quantity
    OrderType type            = OrderType::Limit;
    STPMode   stp_mode        = STPMode::None;
    bool      stp_cancelled   = false;         // set by engine when STP fires on taker

    Order() = default;

    Order(uint64_t id, Side s, int64_t p, uint64_t qty, uint64_t ts,
          OrderType t   = OrderType::Limit,
          uint64_t  pid = 0,
          STPMode   stp = STPMode::None) noexcept
        : order_id(id), side(s), price(p), quantity(qty), remaining(qty),
          timestamp(ts), participant_id(pid), type(t), stp_mode(stp) {}

    // Call after construction to turn this into an iceberg order.
    // Sets remaining = min(ps, quantity) and reserve_qty = quantity - remaining.
    void set_iceberg(uint64_t ps) noexcept {
        peak_size   = ps;
        remaining   = (ps < quantity) ? ps : quantity;
        reserve_qty = quantity - remaining;
    }

    bool is_iceberg() const noexcept { return peak_size > 0; }

    // Reduce remaining by n, return filled amount (min)
    uint64_t fill(uint64_t n) noexcept {
        uint64_t taken = (n <= remaining) ? n : remaining;
        remaining -= taken;
        return taken;
    }

    bool is_filled() const noexcept { return remaining == 0; }
};

} // namespace elob
