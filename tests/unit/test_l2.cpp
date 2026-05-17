#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "marketdata/l2_publisher.hpp"

#include <cstdio>
#include <cstdlib>

using namespace elob;

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); std::exit(1); }
}

static Event make_limit(uint64_t id, Side s, int64_t px, uint64_t qty, uint64_t ts = 0) {
    Order o(id, s, px, qty, ts);
    return Event(id, ts, NewOrder{o});
}
static Event make_iceberg(uint64_t id, Side s, int64_t px,
                           uint64_t total, uint64_t peak, uint64_t ts = 0) {
    Order o(id, s, px, total, ts);
    o.set_iceberg(peak);
    return Event(id, ts, NewOrder{o});
}
static Event make_cancel(uint64_t eid, uint64_t oid, uint64_t ts = 0) {
    return Event(eid, ts, Cancel{oid});
}

// ── Empty book ────────────────────────────────────────────────────────────────

static void test_empty_book() {
    OrderBook    book;
    L2Publisher  pub(book);
    auto snap = pub.build(0);

    check(snap.bid_count == 0, "empty: bid_count must be 0");
    check(snap.ask_count == 0, "empty: ask_count must be 0");
    check(snap.seq_num == 1,   "empty: seq_num starts at 1");
}

// ── Single level each side ────────────────────────────────────────────────────

static void test_single_bid_ask() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);
    L2Publisher    pub(book);

    auto e1 = make_limit(1, Side::Buy,  99, 10, 1);
    auto e2 = make_limit(2, Side::Sell, 101, 15, 2);
    ing.process(e1);
    ing.process(e2);

    auto snap = pub.build(3);
    check(snap.bid_count == 1,        "single: 1 bid level");
    check(snap.bids[0].price == 99,   "single: bid price = 99");
    check(snap.bids[0].qty   == 10,   "single: bid qty = 10");
    check(snap.ask_count == 1,        "single: 1 ask level");
    check(snap.asks[0].price == 101,  "single: ask price = 101");
    check(snap.asks[0].qty   == 15,   "single: ask qty = 15");
}

// ── Multiple levels in correct order ─────────────────────────────────────────

static void test_multiple_levels_ordered() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);
    L2Publisher    pub(book);

    // Insert asks at 103, 101, 102 (out of order) → should appear 101,102,103 in snap
    auto e1 = make_limit(1, Side::Sell, 103, 5, 1);
    auto e2 = make_limit(2, Side::Sell, 101, 10, 2);
    auto e3 = make_limit(3, Side::Sell, 102, 7, 3);
    // Bids at 97, 99, 98
    auto e4 = make_limit(4, Side::Buy, 97, 3, 4);
    auto e5 = make_limit(5, Side::Buy, 99, 8, 5);
    auto e6 = make_limit(6, Side::Buy, 98, 6, 6);
    ing.process(e1); ing.process(e2); ing.process(e3);
    ing.process(e4); ing.process(e5); ing.process(e6);

    auto snap = pub.build(7);

    // Bids: best first (descending) → 99, 98, 97
    check(snap.bid_count == 3,        "ordered: 3 bid levels");
    check(snap.bids[0].price == 99,   "ordered: best bid = 99");
    check(snap.bids[1].price == 98,   "ordered: 2nd bid = 98");
    check(snap.bids[2].price == 97,   "ordered: 3rd bid = 97");

    // Asks: best first (ascending) → 101, 102, 103
    check(snap.ask_count == 3,        "ordered: 3 ask levels");
    check(snap.asks[0].price == 101,  "ordered: best ask = 101");
    check(snap.asks[1].price == 102,  "ordered: 2nd ask = 102");
    check(snap.asks[2].price == 103,  "ordered: 3rd ask = 103");
}

// ── Capped at L2_DEPTH ────────────────────────────────────────────────────────

static void test_depth_capped_at_limit() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);
    L2Publisher    pub(book);

    // Insert 7 ask levels (more than L2_DEPTH=5)
    for (uint64_t i = 1; i <= 7; ++i) {
        auto ev = make_limit(i, Side::Sell, static_cast<int64_t>(100 + i), 10, i);
        ing.process(ev);
    }

    auto snap = pub.build(8);
    check(snap.ask_count == L2_DEPTH, "depth_cap: ask_count must be L2_DEPTH (5)");
    check(snap.asks[0].price == 101,  "depth_cap: best ask = 101");
    check(snap.asks[4].price == 105,  "depth_cap: 5th ask = 105");
}

// ── Iceberg: hidden qty excluded ──────────────────────────────────────────────

static void test_iceberg_hidden_excluded() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);
    L2Publisher    pub(book);

    // Iceberg sell total=100, peak=10 → only 10 should show on L2
    auto e1 = make_iceberg(1, Side::Sell, 100, 100, 10, 1);
    ing.process(e1);

    auto snap = pub.build(2);
    check(snap.ask_count == 1,         "ice_l2: 1 ask level");
    check(snap.asks[0].qty == 10,      "ice_l2: only display qty (10) visible");
    check(snap.asks[0].price == 100,   "ice_l2: price correct");
}

// ── Sequence number monotonic ─────────────────────────────────────────────────

static void test_seq_num_increments() {
    OrderBook   book;
    L2Publisher pub(book);

    auto s1 = pub.build(0);
    auto s2 = pub.build(0);
    auto s3 = pub.build(0);

    check(s1.seq_num == 1, "seq: first = 1");
    check(s2.seq_num == 2, "seq: second = 2");
    check(s3.seq_num == 3, "seq: third = 3");
}

// ── Level disappears after cancel ────────────────────────────────────────────

static void test_cancel_removes_level() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);
    L2Publisher    pub(book);

    auto e1 = make_limit(1, Side::Sell, 100, 10, 1);
    ing.process(e1);
    auto snap1 = pub.build(2);
    check(snap1.ask_count == 1, "cancel_l2: ask present before cancel");

    auto e2 = make_cancel(2, 1, 3);
    ing.process(e2);
    auto snap2 = pub.build(4);
    check(snap2.ask_count == 0, "cancel_l2: ask gone after cancel");
}

// ── Qty aggregated correctly when multiple orders at same level ───────────────

static void test_qty_aggregated_same_level() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);
    L2Publisher    pub(book);

    auto e1 = make_limit(1, Side::Buy, 100, 10, 1);
    auto e2 = make_limit(2, Side::Buy, 100, 20, 2);
    auto e3 = make_limit(3, Side::Buy, 100, 5,  3);
    ing.process(e1); ing.process(e2); ing.process(e3);

    auto snap = pub.build(4);
    check(snap.bid_count == 1,       "agg: 1 bid level");
    check(snap.bids[0].qty == 35,    "agg: total qty = 35");
    check(snap.bids[0].price == 100, "agg: price = 100");
}

int main() {
    test_empty_book();
    test_single_bid_ask();
    test_multiple_levels_ordered();
    test_depth_capped_at_limit();
    test_iceberg_hidden_excluded();
    test_seq_num_increments();
    test_cancel_removes_level();
    test_qty_aggregated_same_level();

    std::puts("All L2 snapshot tests passed.");
    return 0;
}
