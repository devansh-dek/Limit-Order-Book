#include "engine/price_ladder.hpp"

namespace elob {

PriceLadder::PriceLadder(int64_t base_price, int64_t tick_size, int32_t num_ticks, bool is_bid)
    : base_price_(base_price), tick_size_(tick_size), is_bid_(is_bid)
{
    slots_.resize(static_cast<size_t>(num_ticks));
}

LadderSlot* PriceLadder::slot_for(int64_t price) {
    int32_t i = index_of(price);
    if (i < 0 || i >= static_cast<int32_t>(slots_.size())) return nullptr;
    return &slots_[static_cast<size_t>(i)];
}

const LadderSlot* PriceLadder::slot_for(int64_t price) const {
    int32_t i = index_of(price);
    if (i < 0 || i >= static_cast<int32_t>(slots_.size())) return nullptr;
    return &slots_[static_cast<size_t>(i)];
}

LadderSlot* PriceLadder::best() {
    if (best_slot_ == -1) return nullptr;
    return &slots_[static_cast<size_t>(best_slot_)];
}

void PriceLadder::on_insert(int32_t slot_idx, bool became_active) {
    if (became_active) active_count_++;
    if (is_bid_) {
        if (best_slot_ == -1 || slot_idx > best_slot_) best_slot_ = slot_idx;
    } else {
        if (best_slot_ == -1 || slot_idx < best_slot_) best_slot_ = slot_idx;
    }
}

void PriceLadder::on_erase(int32_t slot_idx, bool became_empty) {
    if (became_empty) active_count_--;
    if (!became_empty || slot_idx != best_slot_) return;
    // Book completely empty — skip the scan entirely.
    if (active_count_ == 0) { best_slot_ = -1; return; }
    int32_t n = static_cast<int32_t>(slots_.size());
    best_slot_ = -1;
    if (is_bid_) {
        for (int32_t i = slot_idx - 1; i >= 0; --i)
            if (slots_[static_cast<size_t>(i)].count > 0) { best_slot_ = i; break; }
    } else {
        for (int32_t i = slot_idx + 1; i < n; ++i)
            if (slots_[static_cast<size_t>(i)].count > 0) { best_slot_ = i; break; }
    }
}

int PriceLadder::top_levels(int n, int64_t* out_prices, uint64_t* out_qtys) const {
    if (best_slot_ == -1 || n <= 0) return 0;
    int     written = 0;
    int32_t sz      = static_cast<int32_t>(slots_.size());
    if (is_bid_) {
        for (int32_t i = best_slot_; i >= 0 && written < n; --i) {
            const LadderSlot& s = slots_[static_cast<size_t>(i)];
            if (s.count > 0) {
                out_prices[written] = price_of(i);
                out_qtys[written]   = s.total_qty;
                ++written;
            }
        }
    } else {
        for (int32_t i = best_slot_; i < sz && written < n; ++i) {
            const LadderSlot& s = slots_[static_cast<size_t>(i)];
            if (s.count > 0) {
                out_prices[written] = price_of(i);
                out_qtys[written]   = s.total_qty;
                ++written;
            }
        }
    }
    return written;
}

uint64_t PriceLadder::fillable_qty(int64_t price_limit) const {
    if (best_slot_ == -1) return 0;
    uint64_t total = 0;
    int32_t  limit_idx = index_of(price_limit);
    int32_t  n = static_cast<int32_t>(slots_.size());
    if (is_bid_) {
        // Bids: best is highest; "at-or-better" means price >= price_limit.
        int32_t lo = (limit_idx >= 0) ? limit_idx : 0;
        for (int32_t i = best_slot_; i >= lo; --i)
            total += slots_[static_cast<size_t>(i)].total_qty;
    } else {
        // Asks: best is lowest; "at-or-better" means price <= price_limit.
        int32_t hi = (limit_idx < n) ? limit_idx : n - 1;
        for (int32_t i = best_slot_; i <= hi; ++i)
            total += slots_[static_cast<size_t>(i)].total_qty;
    }
    return total;
}

} // namespace elob
