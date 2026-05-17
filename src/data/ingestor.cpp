#include "data/ingestor.hpp"
#include "data/validator.hpp"

#include <variant>

namespace elob {

std::vector<Trade> EventIngestor::process(Event& ev) {
    std::vector<Trade> trades;

    if (validator_) {
        last_reject_ = validator_->validate(ev);
        if (last_reject_ != RejectReason::None) return trades;
    } else {
        last_reject_ = RejectReason::None;
    }
    // Use std::visit to handle different payloads
    std::visit([&](auto &payload){
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, NewOrder>) {
            Order taker = payload.order; // copy; MatchingEngine mutates taker
            engine_.process(taker, ev.timestamp, trades);
            // Only Limit orders rest on the book. IOC/FOK residuals are discarded.
            // STP CancelNewest/Both sets stp_cancelled; those never rest either.
            // Insert taker directly: preserves participant_id, stp_mode, and type.
            // (taker.remaining already reflects any partial fills from the engine.)
            if (!taker.is_filled() && !taker.stp_cancelled && taker.type == OrderType::Limit) {
                book_.insert(taker);
            }
        } else if constexpr (std::is_same_v<T, Cancel>) {
            book_.cancel(payload.order_id);
        } else if constexpr (std::is_same_v<T, Modify>) {
            book_.modify(payload.order_id, payload.new_price, payload.new_quantity, ev.timestamp);
        } else if constexpr (std::is_same_v<T, Trade>) {


            // trade event in ingestion stream: for now, no-op (could be used for verification/replay)
        } else {
            // monostate or unknown: ignore
        }
    }, ev.payload);

    return trades;
}

} // namespace elob
