#include "data/validator.hpp"

namespace elob {

const char* reject_reason_str(RejectReason r) noexcept {
    switch (r) {
        case RejectReason::None:                  return "None";
        case RejectReason::InvalidPrice:          return "InvalidPrice";
        case RejectReason::InvalidQuantity:       return "InvalidQuantity";
        case RejectReason::DuplicateOrderId:      return "DuplicateOrderId";
        case RejectReason::UnknownOrderId:        return "UnknownOrderId";
        case RejectReason::PriceOutOfRange:       return "PriceOutOfRange";
        case RejectReason::TimestampNotMonotonic: return "TimestampNotMonotonic";
    }
    return "Unknown";
}

RejectReason Validator::validate(const Event& ev) {
    // Monotonic timestamp check applies to every event type.
    if (ev.timestamp < last_timestamp_)
        return reject(RejectReason::TimestampNotMonotonic);
    last_timestamp_ = ev.timestamp;

    return std::visit([&](const auto& payload) -> RejectReason {
        using T = std::decay_t<decltype(payload)>;

        if constexpr (std::is_same_v<T, NewOrder>) {
            const Order& o = payload.order;
            if (o.price    <= 0)                            return reject(RejectReason::InvalidPrice);
            if (o.quantity == 0)                            return reject(RejectReason::InvalidQuantity);
            if (o.price < min_price_ || o.price > max_price_)
                                                            return reject(RejectReason::PriceOutOfRange);
            if (book_.has_order(o.order_id))                return reject(RejectReason::DuplicateOrderId);
            return RejectReason::None;

        } else if constexpr (std::is_same_v<T, Cancel>) {
            if (!book_.has_order(payload.order_id))         return reject(RejectReason::UnknownOrderId);
            return RejectReason::None;

        } else if constexpr (std::is_same_v<T, Modify>) {
            if (!book_.has_order(payload.order_id))         return reject(RejectReason::UnknownOrderId);
            if (payload.new_price    <= 0)                  return reject(RejectReason::InvalidPrice);
            if (payload.new_quantity == 0)                  return reject(RejectReason::InvalidQuantity);
            if (payload.new_price < min_price_ || payload.new_price > max_price_)
                                                            return reject(RejectReason::PriceOutOfRange);
            return RejectReason::None;

        } else {
            // Trade events in the stream and monostate are not validated.
            return RejectReason::None;
        }
    }, ev.payload);
}

} // namespace elob
