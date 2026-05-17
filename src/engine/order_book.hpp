#pragma once

#include "engine/order.hpp"
#include "engine/order_pool.hpp"
#include "engine/price_ladder.hpp"

#include <unordered_map>
#include <cstdint>

namespace elob {

class OrderBook {
public:
    using Price = int64_t;

    // base_price: minimum valid price (inclusive).
    // num_ticks : number of distinct price levels supported.
    // Defaults cover prices 1–19 999 with tick size 1.
    explicit OrderBook(int64_t base_price = 0,
                       int64_t tick_size  = 1,
                       int32_t num_ticks  = 20000);

    void insert(const Order& o);
    bool cancel(uint64_t order_id);
    bool modify(uint64_t order_id, Price new_price, uint64_t new_quantity, uint64_t new_timestamp);

    // Best level access for the matching engine.
    LadderSlot* best_bid();
    LadderSlot* best_ask();

    int64_t bid_price(const LadderSlot* slot) const { return bids_.price_of_slot(slot); }
    int64_t ask_price(const LadderSlot* slot) const { return asks_.price_of_slot(slot); }

    // Pool access: matching engine walks the intrusive linked list via pool indices.
    OrderPool& pool() { return pool_; }

    // Called by the matching engine after slot->total_qty has already been decremented
    // and the order has been filled. Only patches intrusive links + count + best_slot.
    void erase_from_bid_slot(LadderSlot* slot, int32_t pool_idx);
    void erase_from_ask_slot(LadderSlot* slot, int32_t pool_idx);

    // Iceberg replenishment: set maker.remaining = new_display, deduct from reserve_qty,
    // add new_display to slot->total_qty, and move order to tail of the slot's queue
    // (loses time priority — standard exchange behaviour). Does NOT change slot->count.
    void replenish_bid(LadderSlot* slot, int32_t pool_idx, uint64_t new_display);
    void replenish_ask(LadderSlot* slot, int32_t pool_idx, uint64_t new_display);

    // Query helpers used by tests and the L2 publisher (replaces the old find_level API).
    uint64_t qty_at(Side side, Price price) const;
    bool     has_level(Side side, Price price) const;
    bool     has_order(uint64_t order_id)    const;

    // Total qty available to fill a taker at-or-better than price_limit.
    // Used by the matching engine for FOK pre-check without touching the book.
    uint64_t available_to_fill(Side side, Price price_limit) const;

    // Fill parallel arrays with up to n non-empty levels, best-first.
    // Returns count written. Used by L2Publisher to build snapshots.
    int top_bids(int n, int64_t* out_prices, uint64_t* out_qtys) const;
    int top_asks(int n, int64_t* out_prices, uint64_t* out_qtys) const;

private:
    PriceLadder bids_;   // is_bid = true
    PriceLadder asks_;   // is_bid = false
    OrderPool   pool_;

    std::unordered_map<uint64_t, int32_t> order_index_;  // order_id -> pool index

    void append_to_slot(PriceLadder& ladder, LadderSlot* slot, int32_t idx);
    // Detaches from linked list and adjusts total_qty; does NOT free the pool slot.
    void detach_from_slot(PriceLadder& ladder, LadderSlot* slot, int32_t idx);
};

} // namespace elob
