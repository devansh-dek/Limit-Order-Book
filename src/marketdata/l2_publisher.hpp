#pragma once

#include "engine/order_book.hpp"
#include <cstdint>

namespace elob {

static constexpr int L2_DEPTH = 5;

// One aggregated price level for an L2 snapshot.
struct L2Level {
    int64_t  price = 0;
    uint64_t qty   = 0;  // displayed qty only; iceberg hidden reserve excluded
};

// Aggregated top-N bid/ask depth with a monotonically increasing sequence number.
// A receiver that sees seq gap can detect a missed update and re-subscribe.
struct L2Snapshot {
    uint64_t seq_num   = 0;
    uint64_t timestamp = 0;
    L2Level  bids[L2_DEPTH];  // best bid first (descending price)
    L2Level  asks[L2_DEPTH];  // best ask first (ascending price)
    int      bid_count = 0;
    int      ask_count = 0;
};

// Builds L2Snapshot from a live OrderBook on demand.
// Maintains a monotonic seq_num; caller publishes the result over the wire.
class L2Publisher {
public:
    explicit L2Publisher(const OrderBook& book) noexcept : book_(book) {}

    // Build and return the current top-L2_DEPTH bid/ask snapshot.
    // seq_num increments on every call regardless of whether the book changed.
    L2Snapshot build(uint64_t timestamp = 0);

    uint64_t seq_num() const noexcept { return seq_num_; }

private:
    const OrderBook& book_;
    uint64_t seq_num_ = 0;
};

} // namespace elob
