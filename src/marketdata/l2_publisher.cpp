#include "marketdata/l2_publisher.hpp"

namespace elob {

L2Snapshot L2Publisher::build(uint64_t timestamp) {
    L2Snapshot snap;
    snap.seq_num   = ++seq_num_;
    snap.timestamp = timestamp;

    int64_t  prices[L2_DEPTH];
    uint64_t qtys[L2_DEPTH];

    snap.bid_count = book_.top_bids(L2_DEPTH, prices, qtys);
    for (int i = 0; i < snap.bid_count; ++i)
        snap.bids[i] = {prices[i], qtys[i]};

    snap.ask_count = book_.top_asks(L2_DEPTH, prices, qtys);
    for (int i = 0; i < snap.ask_count; ++i)
        snap.asks[i] = {prices[i], qtys[i]};

    return snap;
}

} // namespace elob
