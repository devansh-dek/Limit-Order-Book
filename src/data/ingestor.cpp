#include "data/ingestor.hpp"

#include <variant>

namespace elob {

std::vector<Trade> EventIngestor::process(Event &ev) {
    std::vector<Trade> trades;
    // Use std::visit to handle different payloads
    std::visit([&](auto &payload){
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, NewOrder>) {
            Order taker = payload.order; // copy; MatchingEngine mutates taker
            engine_.process(taker, ev.timestamp, trades);
            // Only Limit orders rest on the book. IOC discards any residual.
            // FOK either filled completely or engine returned with no trades.
            if (!taker.is_filled() && taker.type == OrderType::Limit) {
                Order residual(taker.order_id, taker.side, taker.price, taker.remaining, ev.timestamp);
                book_.insert(residual);
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
