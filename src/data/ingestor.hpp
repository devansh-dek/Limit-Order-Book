// Event ingestion layer: deterministic processing of Event stream
#pragma once

#include "data/event.hpp"
#include "data/validator.hpp"
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"

#include <vector>
#include <cstdint>

namespace elob {

class EventIngestor {
public:
    // validator=nullptr disables pre-trade validation (existing behaviour).
    EventIngestor(OrderBook& book, MatchingEngine& engine,
                  Validator* validator = nullptr) noexcept
        : book_(book), engine_(engine), next_event_id_(1), validator_(validator) {}

    // Process an event and return any trades emitted as a result.
    // Returns empty if the event is rejected by the validator.
    std::vector<Trade> process(Event& ev);

    RejectReason last_reject() const noexcept { return last_reject_; }

private:
    OrderBook&     book_;
    MatchingEngine& engine_;
    uint64_t       next_event_id_;
    Validator*     validator_;
    RejectReason   last_reject_ = RejectReason::None;
};

} // namespace elob
