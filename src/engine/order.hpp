// Order model for EfficientLimitOrderBook
#pragma once

#include <cstdint>
#include <optional>

namespace elob {

enum class Side      : uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : uint8_t { Limit = 0, IOC = 1, FOK = 2 };

struct Order {
    uint64_t  order_id;                        // unique identifier
    Side      side;                            // buy or sell
    int64_t   price;                           // integer price (use integer to avoid FP)
    uint64_t  quantity;                        // original quantity
    uint64_t  remaining;                       // remaining quantity to be filled
    uint64_t  timestamp;                       // monotonic logical timestamp for time-priority
    OrderType type = OrderType::Limit;         // execution instruction

    Order() = default;

    Order(uint64_t id, Side s, int64_t p, uint64_t qty, uint64_t ts,
          OrderType t = OrderType::Limit) noexcept
        : order_id(id), side(s), price(p), quantity(qty), remaining(qty),
          timestamp(ts), type(t) {}

    // Reduce remaining by n, return filled amount (min)
    uint64_t fill(uint64_t n) noexcept {

        uint64_t taken = (n <= remaining) ? n : remaining;
        remaining -= taken;
        return taken;

    }

    bool is_filled() const noexcept { return remaining == 0; }
};

} // namespace elob
