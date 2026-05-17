#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>

using namespace elob;

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); std::exit(1); }
}

static Event make_limit(uint64_t id, Side s, int64_t px, uint64_t qty, uint64_t ts = 0) {
    Order o(id, s, px, qty, ts, OrderType::Limit);
    return Event(id, ts, NewOrder{o});
}
static Event make_ioc(uint64_t id, Side s, int64_t px, uint64_t qty, uint64_t ts = 0) {
    Order o(id, s, px, qty, ts, OrderType::IOC);
    return Event(id, ts, NewOrder{o});
}
static Event make_fok(uint64_t id, Side s, int64_t px, uint64_t qty, uint64_t ts = 0) {
    Order o(id, s, px, qty, ts, OrderType::FOK);
    return Event(id, ts, NewOrder{o});
}

// ── IOC tests ─────────────────────────────────────────────────────────────────

static void test_ioc_full_fill() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto ev1 = make_limit(1, Side::Sell, 100, 10, 1);
    ing.process(ev1);

    auto ev2 = make_ioc(2, Side::Buy, 100, 10, 2);
    auto trades = ing.process(ev2);

    check(trades.size() == 1,       "ioc_full_fill: expected 1 trade");
    check(trades[0].quantity == 10, "ioc_full_fill: wrong qty");
    check(!book.has_level(Side::Sell, 100), "ioc_full_fill: ask should be gone");
    check(!book.has_level(Side::Buy,  100), "ioc_full_fill: no bid should rest");
}

static void test_ioc_partial_residual_discarded() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto ev1 = make_limit(1, Side::Sell, 100, 5, 1);
    ing.process(ev1);

    auto ev2 = make_ioc(2, Side::Buy, 100, 10, 2);
    auto trades = ing.process(ev2);

    check(trades.size() == 1,      "ioc_partial: expected 1 trade");
    check(trades[0].quantity == 5, "ioc_partial: wrong fill qty");
    check(!book.has_level(Side::Buy, 100), "ioc_partial: residual must not rest");
}

static void test_ioc_no_match_discarded() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto ev = make_ioc(1, Side::Buy, 100, 10, 1);
    auto trades = ing.process(ev);

    check(trades.empty(), "ioc_no_match: expected no trades");
    check(!book.has_level(Side::Buy, 100), "ioc_no_match: order must not rest");
}

static void test_ioc_multi_level_partial() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_limit(1, Side::Sell, 100, 5, 1);
    auto e2 = make_limit(2, Side::Sell, 101, 5, 2);
    ing.process(e1);
    ing.process(e2);

    auto e3 = make_ioc(3, Side::Buy, 101, 20, 3);
    auto trades = ing.process(e3);

    check(trades.size() == 2, "ioc_multi: expected 2 trades");
    uint64_t total = trades[0].quantity + trades[1].quantity;
    check(total == 10, "ioc_multi: expected 10 total filled");
    check(!book.has_level(Side::Buy, 101), "ioc_multi: residual must not rest");
}

// ── FOK tests ─────────────────────────────────────────────────────────────────

static void test_fok_exact_fill() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_limit(1, Side::Sell, 100, 10, 1);
    ing.process(e1);

    auto e2 = make_fok(2, Side::Buy, 100, 10, 2);
    auto trades = ing.process(e2);

    check(trades.size() == 1,       "fok_exact: expected 1 trade");
    check(trades[0].quantity == 10, "fok_exact: wrong qty");
    check(!book.has_level(Side::Sell, 100), "fok_exact: ask should be gone");
}

static void test_fok_insufficient_cancelled() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_limit(1, Side::Sell, 100, 5, 1);
    ing.process(e1);

    auto e2 = make_fok(2, Side::Buy, 100, 10, 2);
    auto trades = ing.process(e2);

    check(trades.empty(), "fok_insuff: expected no trades");
    check(book.qty_at(Side::Sell, 100) == 5, "fok_insuff: resting ask must be intact");
    check(!book.has_level(Side::Buy, 100),   "fok_insuff: FOK must not rest");
}

static void test_fok_no_liquidity_cancelled() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto ev = make_fok(1, Side::Buy, 100, 5, 1);
    auto trades = ing.process(ev);

    check(trades.empty(), "fok_no_liq: expected no trades");
    check(!book.has_level(Side::Buy, 100), "fok_no_liq: FOK must not rest");
}

static void test_fok_multi_level_full_fill() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_limit(1, Side::Sell, 100, 5, 1);
    auto e2 = make_limit(2, Side::Sell, 101, 5, 2);
    ing.process(e1);
    ing.process(e2);

    auto e3 = make_fok(3, Side::Buy, 101, 10, 3);
    auto trades = ing.process(e3);

    check(trades.size() == 2, "fok_multi: expected 2 trades");
    uint64_t total = trades[0].quantity + trades[1].quantity;
    check(total == 10, "fok_multi: expected 10 total filled");
}

static void test_fok_multi_level_partial_cancelled() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto e1 = make_limit(1, Side::Sell, 100, 3, 1);
    auto e2 = make_limit(2, Side::Sell, 101, 5, 2);
    ing.process(e1);
    ing.process(e2);

    auto e3 = make_fok(3, Side::Buy, 101, 10, 3);
    auto trades = ing.process(e3);

    check(trades.empty(), "fok_multi_partial: expected no trades");
    check(book.qty_at(Side::Sell, 100) == 3, "fok_multi_partial: level 100 intact");
    check(book.qty_at(Side::Sell, 101) == 5, "fok_multi_partial: level 101 intact");
}

// ── Limit still rests ─────────────────────────────────────────────────────────

static void test_limit_still_rests() {
    OrderBook      book;
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);

    auto ev = make_limit(1, Side::Buy, 99, 10, 1);
    ing.process(ev);

    check(book.has_level(Side::Buy, 99),   "limit_rests: bid should be in book");
    check(book.qty_at(Side::Buy, 99) == 10, "limit_rests: wrong qty");
}

int main() {
    test_ioc_full_fill();
    test_ioc_partial_residual_discarded();
    test_ioc_no_match_discarded();
    test_ioc_multi_level_partial();
    test_fok_exact_fill();
    test_fok_insufficient_cancelled();
    test_fok_no_liquidity_cancelled();
    test_fok_multi_level_full_fill();
    test_fok_multi_level_partial_cancelled();
    test_limit_still_rests();

    std::puts("All market order tests passed.");
    return 0;
}
