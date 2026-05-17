#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "data/validator.hpp"

#include <cstdio>
#include <cstdlib>

using namespace elob;

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); std::exit(1); }
}

// Helper: build a valid NewOrder Event
static Event make_new(uint64_t id, Side s, int64_t px, uint64_t qty, uint64_t ts = 1) {
    Order o(id, s, px, qty, ts);
    return Event(id, ts, NewOrder{o});
}
static Event make_cancel(uint64_t eid, uint64_t oid, uint64_t ts = 2) {
    return Event(eid, ts, Cancel{oid});
}
static Event make_modify(uint64_t eid, uint64_t oid,
                          int64_t new_px, uint64_t new_qty, uint64_t ts = 2) {
    return Event(eid, ts, Modify{oid, new_qty, new_px});
}

// Book with price range [1, 500], so use prices in that range.
static constexpr int64_t MIN_PX = 1;
static constexpr int64_t MAX_PX = 500;

// ── NewOrder validation ───────────────────────────────────────────────────────

static void test_valid_new_order_accepted() {
    OrderBook   book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator   v(book, MIN_PX, MAX_PX);

    auto ev = make_new(1, Side::Buy, 100, 10, 1);
    check(v.validate(ev) == RejectReason::None, "valid_new: must pass");
    check(v.reject_count() == 0,               "valid_new: reject_count = 0");
}

static void test_invalid_price_zero() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto ev = make_new(1, Side::Buy, 0, 10, 1);
    check(v.validate(ev) == RejectReason::InvalidPrice, "zero_px: rejected");
}

static void test_invalid_price_negative() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto ev = make_new(1, Side::Buy, -5, 10, 1);
    check(v.validate(ev) == RejectReason::InvalidPrice, "neg_px: rejected");
}

static void test_invalid_quantity_zero() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto ev = make_new(1, Side::Sell, 100, 0, 1);
    check(v.validate(ev) == RejectReason::InvalidQuantity, "zero_qty: rejected");
}

static void test_price_out_of_range_high() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto ev = make_new(1, Side::Buy, MAX_PX + 1, 10, 1);
    check(v.validate(ev) == RejectReason::PriceOutOfRange, "px_high: rejected");
}

static void test_price_out_of_range_low() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    // Price is valid (> 0) but below min_price_
    auto ev = make_new(1, Side::Buy, MIN_PX - 1, 10, 1);
    // MIN_PX - 1 = 0 → hits InvalidPrice first (price <= 0 check)
    RejectReason r = v.validate(ev);
    check(r == RejectReason::InvalidPrice || r == RejectReason::PriceOutOfRange,
          "px_low: rejected for price or range");
}

static void test_duplicate_order_id() {
    OrderBook      book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    MatchingEngine eng(book);
    Validator      v(book, MIN_PX, MAX_PX);
    EventIngestor  ing(book, eng, &v);

    // First order: accepted and rests on book
    auto e1 = make_new(1, Side::Buy, 100, 10, 1);
    ing.process(e1);
    check(ing.last_reject() == RejectReason::None, "dup: first order accepted");

    // Same id: rejected
    auto e2 = make_new(1, Side::Buy, 101, 5, 2);
    ing.process(e2);
    check(ing.last_reject() == RejectReason::DuplicateOrderId, "dup: second rejected");
    // Book must not be mutated by the rejected order
    check(!book.has_level(Side::Buy, 101), "dup: rejected order not in book");
}

// ── Cancel validation ─────────────────────────────────────────────────────────

static void test_cancel_unknown_id_rejected() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto ev = make_cancel(99, 42, 1);
    check(v.validate(ev) == RejectReason::UnknownOrderId, "cancel_unknown: rejected");
}

static void test_cancel_known_id_accepted() {
    OrderBook      book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    MatchingEngine eng(book);
    Validator      v(book, MIN_PX, MAX_PX);
    EventIngestor  ing(book, eng, &v);

    auto e1 = make_new(1, Side::Sell, 200, 10, 1);
    ing.process(e1);

    auto e2 = make_cancel(2, 1, 2);
    ing.process(e2);
    check(ing.last_reject() == RejectReason::None,      "cancel_known: accepted");
    check(!book.has_level(Side::Sell, 200),             "cancel_known: order removed");
}

// ── Modify validation ─────────────────────────────────────────────────────────

static void test_modify_unknown_id_rejected() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto ev = make_modify(99, 42, 150, 5, 1);
    check(v.validate(ev) == RejectReason::UnknownOrderId, "mod_unknown: rejected");
}

static void test_modify_invalid_new_price() {
    OrderBook      book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    MatchingEngine eng(book);
    Validator      v(book, MIN_PX, MAX_PX);
    EventIngestor  ing(book, eng, &v);

    auto e1 = make_new(1, Side::Buy, 100, 10, 1);
    ing.process(e1);

    auto e2 = make_modify(2, 1, 0, 10, 2);
    ing.process(e2);
    check(ing.last_reject() == RejectReason::InvalidPrice, "mod_px0: rejected");
}

static void test_modify_invalid_new_qty() {
    OrderBook      book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    MatchingEngine eng(book);
    Validator      v(book, MIN_PX, MAX_PX);
    EventIngestor  ing(book, eng, &v);

    auto e1 = make_new(1, Side::Buy, 100, 10, 1);
    ing.process(e1);

    auto e2 = make_modify(2, 1, 100, 0, 2);
    ing.process(e2);
    check(ing.last_reject() == RejectReason::InvalidQuantity, "mod_qty0: rejected");
}

// ── Timestamp monotonicity ────────────────────────────────────────────────────

static void test_timestamp_not_monotonic() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto e1 = make_new(1, Side::Buy, 100, 10, 10);
    v.validate(e1);  // advances last_timestamp_ to 10

    auto e2 = make_new(2, Side::Buy, 101, 10, 5);  // ts=5 < 10
    check(v.validate(e2) == RejectReason::TimestampNotMonotonic, "ts: non-monotonic rejected");
}

static void test_same_timestamp_allowed() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto e1 = make_new(1, Side::Buy, 100, 10, 5);
    v.validate(e1);

    // Same timestamp is allowed (>=, not >)
    auto e2 = make_new(2, Side::Sell, 200, 10, 5);
    check(v.validate(e2) == RejectReason::None, "ts_same: same timestamp OK");
}

// ── Reject count and stats ────────────────────────────────────────────────────

static void test_reject_count_accumulates() {
    OrderBook book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    Validator v(book, MIN_PX, MAX_PX);

    auto e1 = make_new(1, Side::Buy, 0,   10, 1);  // InvalidPrice
    auto e2 = make_new(2, Side::Buy, 100, 0,  2);  // InvalidQuantity
    v.validate(e1);
    v.validate(e2);

    check(v.reject_count() == 2, "count: 2 rejections counted");
}

// ── Ingestor without validator: existing behaviour unchanged ──────────────────

static void test_no_validator_accepts_everything() {
    OrderBook      book(MIN_PX, 1, static_cast<int32_t>(MAX_PX));
    MatchingEngine eng(book);
    EventIngestor  ing(book, eng);  // no validator

    // Even a duplicate ID passes through without a validator
    auto e1 = make_new(1, Side::Buy, 100, 10, 1);
    auto e2 = make_new(1, Side::Buy, 101, 10, 2);  // same id, no validator
    ing.process(e1);
    ing.process(e2);
    check(ing.last_reject() == RejectReason::None, "no_validator: always None");
}

int main() {
    test_valid_new_order_accepted();
    test_invalid_price_zero();
    test_invalid_price_negative();
    test_invalid_quantity_zero();
    test_price_out_of_range_high();
    test_price_out_of_range_low();
    test_duplicate_order_id();
    test_cancel_unknown_id_rejected();
    test_cancel_known_id_accepted();
    test_modify_unknown_id_rejected();
    test_modify_invalid_new_price();
    test_modify_invalid_new_qty();
    test_timestamp_not_monotonic();
    test_same_timestamp_allowed();
    test_reject_count_accumulates();
    test_no_validator_accepts_everything();

    std::puts("All validator tests passed.");
    return 0;
}
