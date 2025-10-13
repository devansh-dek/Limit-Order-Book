// Event ingestion layer: deterministic processing of Event stream
#pragma once

#include "data/event.hpp"
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"

#include <vector>
#include <cstdint>

namespace elob {

class EventIngestor {
public:
    EventIngestor(OrderBook &book, MatchingEngine &engine) noexcept
        : book_(book), engine_(engine), next_event_id_(1) {}

    // Process an event and return any trades emitted as a result.
    std::vector<Trade> process(Event &ev);

private:
    OrderBook &book_;
    MatchingEngine &engine_;
    uint64_t next_event_id_;
};

} // namespace elob
