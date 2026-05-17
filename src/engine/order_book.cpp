#include "engine/order_book.hpp"

namespace elob {

OrderBook::OrderBook(int64_t base_price, int64_t tick_size, int32_t num_ticks)
    : bids_(base_price, tick_size, num_ticks, /*is_bid=*/true)
    , asks_(base_price, tick_size, num_ticks, /*is_bid=*/false)
{}

// ── private helpers ──────────────────────────────────────────────────────────

void OrderBook::append_to_slot(PriceLadder& ladder, LadderSlot* slot, int32_t idx) {
    PooledOrder& po = pool_.at(idx);
    po.prev = slot->tail;
    po.next = -1;
    if (slot->tail != -1) pool_.at(slot->tail).next = idx;
    else                  slot->head = idx;
    slot->tail = idx;
    slot->count++;
    bool became_active = (slot->count == 1);
    slot->total_qty += po.data.remaining;
    ladder.on_insert(ladder.index_of_slot(slot), became_active);
}

void OrderBook::detach_from_slot(PriceLadder& ladder, LadderSlot* slot, int32_t idx) {
    PooledOrder& po = pool_.at(idx);
    if (po.prev != -1) pool_.at(po.prev).next = po.next;
    else               slot->head = po.next;
    if (po.next != -1) pool_.at(po.next).prev = po.prev;
    else               slot->tail = po.prev;
    slot->total_qty -= po.data.remaining;
    slot->count--;
    ladder.on_erase(ladder.index_of_slot(slot), slot->count == 0);
}

// ── public interface ─────────────────────────────────────────────────────────

void OrderBook::insert(const Order& o) {
    PriceLadder& ladder = (o.side == Side::Buy) ? bids_ : asks_;
    LadderSlot*  slot   = ladder.slot_for(o.price);
    if (!slot) return;  // price out of range; validator will catch this in Step 8
    int32_t idx = pool_.alloc(o);
    append_to_slot(ladder, slot, idx);
    order_index_.emplace(o.order_id, idx);
}

bool OrderBook::cancel(uint64_t order_id) {
    auto it = order_index_.find(order_id);
    if (it == order_index_.end()) return false;
    int32_t      idx    = it->second;
    PooledOrder& po     = pool_.at(idx);
    PriceLadder& ladder = (po.data.side == Side::Buy) ? bids_ : asks_;
    LadderSlot*  slot   = ladder.slot_for(po.data.price);
    detach_from_slot(ladder, slot, idx);
    pool_.free(idx);
    order_index_.erase(it);
    return true;
}

bool OrderBook::modify(uint64_t order_id, Price new_price, uint64_t new_quantity, uint64_t new_timestamp) {
    auto it = order_index_.find(order_id);
    if (it == order_index_.end()) return false;

    int32_t      idx    = it->second;
    PooledOrder& po     = pool_.at(idx);
    Order&       ord    = po.data;
    PriceLadder& ladder = (ord.side == Side::Buy) ? bids_ : asks_;

    uint64_t filled        = ord.quantity - ord.remaining;
    uint64_t new_remaining = (new_quantity > filled) ? (new_quantity - filled) : 0;

    if (ord.price == new_price) {
        // Same price: update quantity in-place, no movement in the queue.
        LadderSlot* slot  = ladder.slot_for(ord.price);
        slot->total_qty  -= ord.remaining;
        slot->total_qty  += new_remaining;
        ord.quantity      = new_quantity;
        ord.remaining     = new_remaining;
        ord.timestamp     = new_timestamp;
        return true;
    }

    // Price change: detach from old slot, update, reattach at tail of new slot.
    // The order loses time priority at the new price level — standard exchange behaviour.
    LadderSlot* old_slot = ladder.slot_for(ord.price);
    detach_from_slot(ladder, old_slot, idx);

    ord.price     = new_price;
    ord.quantity  = new_quantity;
    ord.remaining = new_remaining;
    ord.timestamp = new_timestamp;
    po.prev = po.next = -1;

    LadderSlot* new_slot = ladder.slot_for(new_price);
    if (!new_slot) {
        // new price out of range — discard order
        pool_.free(idx);
        order_index_.erase(it);
        return false;
    }
    append_to_slot(ladder, new_slot, idx);
    return true;
}

// Called by the matching engine only. By this point the matching engine has already
// decremented slot->total_qty by the fill quantity, so we only patch the intrusive
// links, count, and best_slot tracking.
void OrderBook::erase_from_bid_slot(LadderSlot* slot, int32_t pool_idx) {
    PooledOrder& po = pool_.at(pool_idx);
    if (po.prev != -1) pool_.at(po.prev).next = po.next;
    else               slot->head = po.next;
    if (po.next != -1) pool_.at(po.next).prev = po.prev;
    else               slot->tail = po.prev;
    slot->count--;
    bids_.on_erase(bids_.index_of_slot(slot), slot->count == 0);
    order_index_.erase(po.data.order_id);
    pool_.free(pool_idx);
}

void OrderBook::erase_from_ask_slot(LadderSlot* slot, int32_t pool_idx) {
    PooledOrder& po = pool_.at(pool_idx);
    if (po.prev != -1) pool_.at(po.prev).next = po.next;
    else               slot->head = po.next;
    if (po.next != -1) pool_.at(po.next).prev = po.prev;
    else               slot->tail = po.prev;
    slot->count--;
    asks_.on_erase(asks_.index_of_slot(slot), slot->count == 0);
    order_index_.erase(po.data.order_id);
    pool_.free(pool_idx);
}

LadderSlot* OrderBook::best_bid() { return bids_.best(); }
LadderSlot* OrderBook::best_ask() { return asks_.best(); }

uint64_t OrderBook::qty_at(Side side, Price price) const {
    const PriceLadder& ladder = (side == Side::Buy) ? bids_ : asks_;
    const LadderSlot*  slot   = ladder.slot_for(price);
    return slot ? slot->total_qty : 0;
}

bool OrderBook::has_level(Side side, Price price) const {
    const PriceLadder& ladder = (side == Side::Buy) ? bids_ : asks_;
    const LadderSlot*  slot   = ladder.slot_for(price);
    return slot && slot->count > 0;
}

uint64_t OrderBook::available_to_fill(Side side, Price price_limit) const {
    // Buy taker crosses asks (price_limit = max willing to pay).
    // Sell taker crosses bids (price_limit = min willing to accept).
    if (side == Side::Buy) return asks_.fillable_qty(price_limit);
    else                   return bids_.fillable_qty(price_limit);
}

} // namespace elob
