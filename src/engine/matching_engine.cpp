#include "engine/matching_engine.hpp"

namespace elob {

static inline uint64_t min_u64(uint64_t a, uint64_t b) noexcept { return a < b ? a : b; }

void MatchingEngine::match_buy(Order& taker, uint64_t timestamp, std::vector<Trade>& out_trades) {
    while (!taker.is_filled()) {
        LadderSlot* slot = book_.best_ask();
        if (!slot) break;
        int64_t slot_price = book_.ask_price(slot);
        if (slot_price > taker.price) break;

        // Walk the intrusive list at this price level (FIFO order).
        // Save next_idx before any erase because erase patches the links.
        int32_t idx = slot->head;
        while (idx != -1 && !taker.is_filled()) {
            PooledOrder& po       = book_.pool().at(idx);
            int32_t      next_idx = po.next;
            Order&       maker    = po.data;

            // Self-trade prevention: taker and maker share a participant_id.
            if (taker.stp_mode != STPMode::None &&
                maker.participant_id == taker.participant_id) {
                if (taker.stp_mode == STPMode::CancelNewest) {
                    taker.stp_cancelled = true;
                    return;  // taker cancelled; resting book untouched
                }
                // CancelOldest or CancelBoth: remove the maker from the book.
                slot->total_qty -= maker.remaining;
                book_.erase_from_ask_slot(slot, idx);
                if (taker.stp_mode == STPMode::CancelBoth) {
                    taker.stp_cancelled = true;
                    return;
                }
                idx = next_idx;
                continue;
            }

            uint64_t qty   = min_u64(taker.remaining, maker.remaining);
            slot->total_qty -= qty;   // kept in sync before fill mutates remaining
            maker.fill(qty);
            taker.fill(qty);
            out_trades.emplace_back(next_trade_id_++, maker.order_id, taker.order_id,
                                    slot_price, qty, timestamp);

            if (maker.is_filled()) {
                if (maker.is_iceberg() && maker.reserve_qty > 0) {
                    // Replenish: next slice goes to tail (loses time priority).
                    uint64_t new_display = min_u64(maker.peak_size, maker.reserve_qty);
                    book_.replenish_ask(slot, idx, new_display);
                } else {
                    book_.erase_from_ask_slot(slot, idx);
                }
            }

            idx = next_idx;
        }
        // If the slot is now empty, best_ask() will return the next valid slot on the
        // next outer iteration (on_erase already updated best_slot_ inside erase_from_ask_slot).
    }
}

void MatchingEngine::match_sell(Order& taker, uint64_t timestamp, std::vector<Trade>& out_trades) {
    while (!taker.is_filled()) {
        LadderSlot* slot = book_.best_bid();
        if (!slot) break;
        int64_t slot_price = book_.bid_price(slot);
        if (slot_price < taker.price) break;

        int32_t idx = slot->head;
        while (idx != -1 && !taker.is_filled()) {
            PooledOrder& po       = book_.pool().at(idx);
            int32_t      next_idx = po.next;
            Order&       maker    = po.data;

            if (taker.stp_mode != STPMode::None &&
                maker.participant_id == taker.participant_id) {
                if (taker.stp_mode == STPMode::CancelNewest) {
                    taker.stp_cancelled = true;
                    return;
                }
                slot->total_qty -= maker.remaining;
                book_.erase_from_bid_slot(slot, idx);
                if (taker.stp_mode == STPMode::CancelBoth) {
                    taker.stp_cancelled = true;
                    return;
                }
                idx = next_idx;
                continue;
            }

            uint64_t qty   = min_u64(taker.remaining, maker.remaining);
            slot->total_qty -= qty;
            maker.fill(qty);
            taker.fill(qty);
            out_trades.emplace_back(next_trade_id_++, maker.order_id, taker.order_id,
                                    slot_price, qty, timestamp);

            if (maker.is_filled()) {
                if (maker.is_iceberg() && maker.reserve_qty > 0) {
                    uint64_t new_display = min_u64(maker.peak_size, maker.reserve_qty);
                    book_.replenish_bid(slot, idx, new_display);
                } else {
                    book_.erase_from_bid_slot(slot, idx);
                }
            }

            idx = next_idx;
        }
    }
}

} // namespace elob
