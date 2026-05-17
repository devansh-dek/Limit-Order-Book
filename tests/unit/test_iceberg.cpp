#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"

#include <cstdio>
#include <cstdlib>

using namespace elob;

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); std::exit(1); }
}

// Build an iceberg order and wrap it in an Event.
static Event make_iceberg(uint64_t id, Side s, int64_t px,
                           uint64_t total_qty, uint64_t peak,
                           uint64_t ts = 0) {
    Order o(id, s, px, total_qty, ts);
    o.set_iceberg(peak);
    return Event(id, ts, NewOrder{o});
}

static Event make_limit(uint64_t id, Side s, int64_t px, uint64_t qty, uint64_t ts = 0) {
    Order o(id, s, px, qty, ts);
    return Event(id, ts, NewOrder{o});
}

// ── L2 visibility ─────────────────────────────────────────────────────────────

static void test_hidden_qty_not_in_total_qty() {
    // Iceberg sell: total=100, peak=10 → only 10 visible on L2.
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_iceberg(1, Side::Sell, 100, 100, 10, 1);
    ing.process(e1);

    check(book.qty_at(Side::Sell, 100) == 10,
          "hidden: only display qty (10) should appear in total_qty");
    check(book.has_level(Side::Sell, 100), "hidden: level should exist");
}

// ── Replenishment on full slice fill ──────────────────────────────────────────

static void test_slice_replenishes_after_fill() {
    // total=30, peak=10 → display=10, reserve=20.
    // Taker buys 10 → slice filled, replenishes 10 → display=10, reserve=10.
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_iceberg(1, Side::Sell, 100, 30, 10, 1);
    ing.process(e1);

    auto e2 = make_limit(2, Side::Buy, 100, 10, 2);
    auto trades = ing.process(e2);

    check(trades.size() == 1,       "replenish: expected 1 trade");
    check(trades[0].quantity == 10, "replenish: wrong fill qty");
    // After replenishment: new slice = min(10, 20) = 10 displayed.
    check(book.qty_at(Side::Sell, 100) == 10, "replenish: next slice should be visible");
    check(book.has_level(Side::Sell, 100),    "replenish: level must still exist");
}

static void test_iceberg_fully_consumed() {
    // total=20, peak=10 → 2 slices. Taker buys 20 → fully consumed.
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_iceberg(1, Side::Sell, 100, 20, 10, 1);
    ing.process(e1);

    auto e2 = make_limit(2, Side::Buy, 100, 20, 2);
    auto trades = ing.process(e2);

    check(trades.size() == 2, "exhaust: expected 2 trades (one per slice)");
    uint64_t total = trades[0].quantity + trades[1].quantity;
    check(total == 20, "exhaust: wrong total fill");
    check(!book.has_level(Side::Sell, 100), "exhaust: iceberg should be gone");
}

// ── Priority: replenished slice goes to tail ──────────────────────────────────

static void test_replenished_slice_loses_priority() {
    // At price 100: [iceberg(id=1, peak=10)] then [plain(id=2, qty=10)].
    // Taker buys 10 → fills iceberg's first slice; iceberg replenishes and moves to TAIL.
    // Taker buys 10 more → should fill plain order (id=2) next, not the replenished iceberg.
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_iceberg(1, Side::Sell, 100, 20, 10, 1);
    auto e2 = make_limit(2,  Side::Sell, 100, 10, 2);
    ing.process(e1);
    ing.process(e2);

    // total displayed = 10 (iceberg slice) + 10 (plain) = 20
    check(book.qty_at(Side::Sell, 100) == 20, "priority: initial display qty");

    // Taker buys 20: should fill slice 1 of iceberg (10), then plain order (10).
    auto e3 = make_limit(3, Side::Buy, 100, 20, 3);
    auto trades = ing.process(e3);

    check(trades.size() == 2, "priority: expected 2 trades");
    // First trade against iceberg (order 1), second against plain (order 2).
    check(trades[0].maker_order_id == 1, "priority: first trade must be vs iceberg");
    check(trades[1].maker_order_id == 2, "priority: second trade must be vs plain order");
    // Iceberg's second slice should still be resting at the back.
    check(book.qty_at(Side::Sell, 100) == 10, "priority: replenished slice still visible");
}

static void test_replenished_slice_then_filled() {
    // Continue from above scenario: now buy the remaining iceberg slice.
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_iceberg(1, Side::Sell, 100, 20, 10, 1);
    auto e2 = make_limit(2,  Side::Sell, 100, 10, 2);
    ing.process(e1);
    ing.process(e2);

    auto e3 = make_limit(3, Side::Buy, 100, 20, 3);
    ing.process(e3);
    // Iceberg's second slice (10) is now at head; buy it.
    auto e4 = make_limit(4, Side::Buy, 100, 10, 4);
    auto trades = ing.process(e4);

    check(trades.size() == 1, "replenish_tail: expected 1 trade");
    check(trades[0].maker_order_id == 1, "replenish_tail: should fill vs iceberg second slice");
    check(!book.has_level(Side::Sell, 100), "replenish_tail: book empty after");
}

// ── Partial fill of a slice ───────────────────────────────────────────────────

static void test_partial_slice_fill_no_replenish() {
    // Taker fills only part of the display slice — no replenishment yet.
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_iceberg(1, Side::Sell, 100, 30, 10, 1);
    ing.process(e1);

    auto e2 = make_limit(2, Side::Buy, 100, 5, 2);
    auto trades = ing.process(e2);

    check(trades.size() == 1,      "partial_slice: 1 trade");
    check(trades[0].quantity == 5, "partial_slice: wrong fill");
    // Displayed remaining should be 5 (slice is now 5/10), no replenishment.
    check(book.qty_at(Side::Sell, 100) == 5, "partial_slice: display qty = slice - filled = 5");
}

// ── Bid-side iceberg ──────────────────────────────────────────────────────────

static void test_bid_iceberg_replenishes() {
    // Iceberg buy: total=30, peak=10.
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_iceberg(1, Side::Buy, 100, 30, 10, 1);
    ing.process(e1);
    check(book.qty_at(Side::Buy, 100) == 10, "bid_ice: display = 10");

    // Sell 10 → fills slice, replenishes.
    auto e2 = make_limit(2, Side::Sell, 100, 10, 2);
    auto trades = ing.process(e2);

    check(trades.size() == 1,       "bid_ice: 1 trade");
    check(trades[0].quantity == 10, "bid_ice: fill = 10");
    check(book.qty_at(Side::Buy, 100) == 10, "bid_ice: replenished display = 10");
}

// ── Plain order regression ────────────────────────────────────────────────────

static void test_plain_order_unaffected() {
    OrderBook book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_limit(1, Side::Sell, 100, 50, 1);
    ing.process(e1);

    check(book.qty_at(Side::Sell, 100) == 50, "plain: full qty visible");

    auto e2 = make_limit(2, Side::Buy, 100, 50, 2);
    auto trades = ing.process(e2);

    check(trades.size() == 1,       "plain: 1 trade");
    check(trades[0].quantity == 50, "plain: full fill");
    check(!book.has_level(Side::Sell, 100), "plain: empty after");
}

int main() {
    test_hidden_qty_not_in_total_qty();
    test_slice_replenishes_after_fill();
    test_iceberg_fully_consumed();
    test_replenished_slice_loses_priority();
    test_replenished_slice_then_filled();
    test_partial_slice_fill_no_replenish();
    test_bid_iceberg_replenishes();
    test_plain_order_unaffected();

    std::puts("All iceberg tests passed.");
    return 0;
}
