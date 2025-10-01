// Event model for EfficientLimitOrderBook
#pragma once

#include "elob/order.hpp"
#include "elob/trade.hpp"

#include <variant>
#include <cstdint>

namespace elob {

struct NewOrder { Order order; };
struct Cancel { uint64_t order_id; };
struct Modify { uint64_t order_id; uint64_t new_quantity; int64_t new_price; };

using EventPayload = std::variant<std::monostate, NewOrder, Cancel, Modify, Trade>;

struct Event {
    uint64_t event_id;    // unique event id
    uint64_t timestamp;   // logical timestamp
    EventPayload payload; // one of the payload types

    Event() = default;
    Event(uint64_t id, uint64_t ts, EventPayload p) noexcept : event_id(id), timestamp(ts), payload(std::move(p)) {}
};

} // namespace elob
