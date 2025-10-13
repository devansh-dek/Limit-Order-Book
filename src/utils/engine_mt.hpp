// Thread-safe wrapper for matching engine (optional concurrency)
#pragma once

#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "engine/trade.hpp"
#include "data/event.hpp"

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
