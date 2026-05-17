#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"

#include <cstdio>
#include <cstdlib>

using namespace elob;

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); std::exit(1); }
}

// pid=0 means no participant (default); stp=None by default.
static Event make_order(uint64_t id, Side s, int64_t px, uint64_t qty, uint64_t ts,
                         uint64_t pid = 0, STPMode stp = STPMode::None) {
    Order o(id, s, px, qty, ts, OrderType::Limit, pid, stp);
    return Event(id, ts, NewOrder{o});
}

// ── CancelNewest ──────────────────────────────────────────────────────────────

static void test_cn_taker_cancelled_maker_intact() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    // Participant 1 posts a resting ask at 100 qty=10.
    auto e1 = make_order(1, Side::Sell, 100, 10, 1, /*pid=*/1);
    ing.process(e1);

    // Same participant sends a buy IOC-like taker with CancelNewest — should be cancelled.
    auto e2 = make_order(2, Side::Buy, 100, 10, 2, /*pid=*/1, STPMode::CancelNewest);
    auto trades = ing.process(e2);

    check(trades.empty(),                        "cn: no trades expected");
    check(book.qty_at(Side::Sell, 100) == 10,   "cn: resting ask must be intact");
    check(!book.has_level(Side::Buy,  100),      "cn: taker must not rest on book");
}

static void test_cn_cross_participant_fills_normally() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_order(1, Side::Sell, 100, 10, 1, /*pid=*/1);
    ing.process(e1);

    // Different participant — STP should not fire; normal fill.
    auto e2 = make_order(2, Side::Buy, 100, 10, 2, /*pid=*/2, STPMode::CancelNewest);
    auto trades = ing.process(e2);

    check(trades.size() == 1,       "cn_cross: expected 1 trade");
    check(trades[0].quantity == 10, "cn_cross: wrong fill qty");
}

static void test_cn_self_skip_then_cross_fills() {
    // Same-participant maker at head; different-participant maker behind it.
    // CancelNewest fires on the first, so taker is cancelled immediately —
    // it does NOT skip to the cross-participant order.
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_order(1, Side::Sell, 100, 5, 1, /*pid=*/1);  // self
    auto e2 = make_order(2, Side::Sell, 100, 5, 2, /*pid=*/2);  // cross
    ing.process(e1);
    ing.process(e2);

    auto e3 = make_order(3, Side::Buy, 100, 5, 3, /*pid=*/1, STPMode::CancelNewest);
    auto trades = ing.process(e3);

    check(trades.empty(), "cn_skip: CancelNewest fires immediately, no trades");
    check(book.qty_at(Side::Sell, 100) == 10, "cn_skip: both resting asks intact");
}

// ── CancelOldest ─────────────────────────────────────────────────────────────

static void test_co_maker_cancelled_taker_continues() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    // Self maker at 100, then cross maker at 100 behind it.
    auto e1 = make_order(1, Side::Sell, 100, 5, 1, /*pid=*/1);  // self, head
    auto e2 = make_order(2, Side::Sell, 100, 5, 2, /*pid=*/2);  // cross, tail
    ing.process(e1);
    ing.process(e2);

    // CancelOldest: self maker (e1) is removed, taker fills against cross maker (e2).
    auto e3 = make_order(3, Side::Buy, 100, 5, 3, /*pid=*/1, STPMode::CancelOldest);
    auto trades = ing.process(e3);

    check(trades.size() == 1,       "co: expected 1 trade against cross maker");
    check(trades[0].quantity == 5,  "co: wrong fill qty");
    check(trades[0].maker_order_id == 2, "co: trade must be against order 2");
    check(!book.has_level(Side::Sell, 100), "co: book should be empty");
}

static void test_co_maker_cancelled_book_intact_qty() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_order(1, Side::Sell, 100, 10, 1, /*pid=*/1);
    ing.process(e1);

    // Only self maker — CO cancels it, taker has nothing left to fill → rests on book.
    auto e2 = make_order(2, Side::Buy, 100, 10, 2, /*pid=*/1, STPMode::CancelOldest);
    auto trades = ing.process(e2);

    check(trades.empty(), "co_only: no trades expected");
    check(!book.has_level(Side::Sell, 100), "co_only: self maker cancelled");
    // Taker was not stp_cancelled (CO doesn't cancel taker), so it rests as Limit.
    check(book.has_level(Side::Buy, 100),   "co_only: taker should rest on book");
    check(book.qty_at(Side::Buy, 100) == 10, "co_only: taker qty wrong");
}

static void test_co_multiple_self_makers_all_cancelled() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    // Three self makers, taker wants 30 — all self, so all get CO-cancelled.
    auto e1 = make_order(1, Side::Sell, 100, 10, 1, /*pid=*/1);
    auto e2 = make_order(2, Side::Sell, 100, 10, 2, /*pid=*/1);
    auto e3 = make_order(3, Side::Sell, 100, 10, 3, /*pid=*/1);
    ing.process(e1); ing.process(e2); ing.process(e3);

    auto e4 = make_order(4, Side::Buy, 100, 30, 4, /*pid=*/1, STPMode::CancelOldest);
    auto trades = ing.process(e4);

    check(trades.empty(), "co_multi: no trades");
    check(!book.has_level(Side::Sell, 100), "co_multi: all self asks cancelled");
    check(book.has_level(Side::Buy, 100),   "co_multi: taker rests on book");
}

// ── CancelBoth ───────────────────────────────────────────────────────────────

static void test_cb_both_cancelled() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_order(1, Side::Sell, 100, 10, 1, /*pid=*/1);
    ing.process(e1);

    auto e2 = make_order(2, Side::Buy, 100, 10, 2, /*pid=*/1, STPMode::CancelBoth);
    auto trades = ing.process(e2);

    check(trades.empty(),                       "cb: no trades expected");
    check(!book.has_level(Side::Sell, 100),     "cb: maker must be cancelled");
    check(!book.has_level(Side::Buy,  100),     "cb: taker must not rest");
}

static void test_cb_cross_then_self_fills_cross_cancels_on_self() {
    // Cross maker at head, self maker behind. Taker fills cross, then hits self → CB fires.
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_order(1, Side::Sell, 100, 5, 1, /*pid=*/2);  // cross, head
    auto e2 = make_order(2, Side::Sell, 100, 5, 2, /*pid=*/1);  // self, tail
    ing.process(e1);
    ing.process(e2);

    // Taker wants 10: fills 5 vs cross, then CB fires on self maker.
    auto e3 = make_order(3, Side::Buy, 100, 10, 3, /*pid=*/1, STPMode::CancelBoth);
    auto trades = ing.process(e3);

    check(trades.size() == 1,       "cb_mixed: 1 trade vs cross maker");
    check(trades[0].quantity == 5,  "cb_mixed: wrong fill qty");
    check(!book.has_level(Side::Sell, 100), "cb_mixed: self maker cancelled");
    check(!book.has_level(Side::Buy,  100), "cb_mixed: taker not rested");
}

// ── STPMode::None (no STP) ───────────────────────────────────────────────────

static void test_stp_none_self_trade_allowed() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_order(1, Side::Sell, 100, 10, 1, /*pid=*/1, STPMode::None);
    ing.process(e1);

    // Same participant but stp_mode=None → normal fill.
    auto e2 = make_order(2, Side::Buy, 100, 10, 2, /*pid=*/1, STPMode::None);
    auto trades = ing.process(e2);

    check(trades.size() == 1,       "stp_none: expected 1 trade");
    check(trades[0].quantity == 10, "stp_none: wrong qty");
}

int main() {
    test_cn_taker_cancelled_maker_intact();
    test_cn_cross_participant_fills_normally();
    test_cn_self_skip_then_cross_fills();
    test_co_maker_cancelled_taker_continues();
    test_co_maker_cancelled_book_intact_qty();
    test_co_multiple_self_makers_all_cancelled();
    test_cb_both_cancelled();
    test_cb_cross_then_self_fills_cross_cancels_on_self();
    test_stp_none_self_trade_allowed();

    std::puts("All STP tests passed.");
    return 0;
}
