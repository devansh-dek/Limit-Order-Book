#pragma once

#include <vector>
#include <cstdint>

namespace elob {

// One entry per price tick. Kept compact (20 bytes) for cache-friendly iteration.
struct LadderSlot {
    int32_t  head      = -1;   // pool index of first order (FIFO head)
    int32_t  tail      = -1;   // pool index of last order  (FIFO tail)
    uint32_t count     = 0;    // number of live orders
    uint64_t total_qty = 0;    // total visible quantity (for L2 snapshots)
};

// Flat array of LadderSlots indexed by (price - base_price) / tick_size.
// is_bid=true  → best is highest non-empty slot (bids sorted descending)
// is_bid=false → best is lowest non-empty slot  (asks sorted ascending)
class PriceLadder {
public:
    PriceLadder(int64_t base_price, int64_t tick_size, int32_t num_ticks, bool is_bid);

    LadderSlot*       slot_for(int64_t price);
    const LadderSlot* slot_for(int64_t price) const;

    // Best active slot; nullptr if the ladder is empty.
    LadderSlot* best();

    // Fill out_prices/out_qtys with up to n non-empty levels, best-first.
    // Returns number of levels written. Used by L2Publisher.
    int top_levels(int n, int64_t* out_prices, uint64_t* out_qtys) const;

    // Sum of total_qty across all slots at prices at-or-better than price_limit.
    // For asks (is_bid=false): slots with price <= price_limit.
    // For bids (is_bid=true):  slots with price >= price_limit.
    uint64_t fillable_qty(int64_t price_limit) const;

    // Must be called after inserting an order into a slot.
    // became_active=true when the slot's count just went from 0 to 1.
    void on_insert(int32_t slot_idx, bool became_active);
    // Must be called after removing an order; became_empty=true when count hits 0.
    void on_erase(int32_t slot_idx, bool became_empty);

    int64_t price_of(int32_t slot_idx)          const { return base_price_ + slot_idx * tick_size_; }
    int64_t price_of_slot(const LadderSlot* s)  const { return price_of(index_of_slot(s)); }
    int32_t index_of(int64_t price)             const { return static_cast<int32_t>((price - base_price_) / tick_size_); }
    int32_t index_of_slot(const LadderSlot* s)  const { return static_cast<int32_t>(s - slots_.data()); }
    bool    in_range(int64_t price)             const {
        int32_t i = index_of(price);
        return i >= 0 && i < static_cast<int32_t>(slots_.size());
    }

private:
    std::vector<LadderSlot> slots_;
    int64_t base_price_;
    int64_t tick_size_;
    bool    is_bid_;
    int32_t best_slot_    = -1;
    int32_t active_count_ = 0;  // number of slots with count > 0
};

} // namespace elob
