// Thread-safe wrapper for matching engine (optional concurrency)
#pragma once

#include "elob/order_book.hpp"
#include "elob/matching_engine.hpp"
#include "elob/ingestor.hpp"
#include "elob/trade.hpp"
#include "elob/event.hpp"

#include <mutex>
#include <vector>

namespace elob {

// Simple thread-safe wrapper: all operations protected by a single mutex.
// For determinism, caller must ensure events are processed in sequence order.
class EngineMultiThreaded {
public:
    EngineMultiThreaded() : book_(), engine_(book_), ingestor_(book_, engine_) {}

    std::vector<Trade> process_event(Event &ev) {
        std::lock_guard<std::mutex> lock(mtx_);
        return ingestor_.process(ev);
    }

private:
    std::mutex mtx_;
    OrderBook book_;
    MatchingEngine engine_;
    EventIngestor ingestor_;
};

} // namespace elob
