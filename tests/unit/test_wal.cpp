#include "logging/wal_logger.hpp"
#include "engine/order.hpp"
#include "engine/trade.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

using namespace elob;

static void check(bool cond, const char* msg) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); std::exit(1); }
}

// Read n records from a binary WAL file into out[].
static int read_wal(const char* path, WalRecord* out, int max) {
    int fd = ::open(path, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = ::read(fd, out, static_cast<std::size_t>(max) * sizeof(WalRecord));
    ::close(fd);
    if (n < 0) return 0;
    return static_cast<int>(n / static_cast<ssize_t>(sizeof(WalRecord)));
}

static const char* TMP = "/tmp/elob_test_wal.bin";

// ── Record layout and content ─────────────────────────────────────────────────

static void test_new_order_record() {
    {
        WalLogger wal(TMP);
        Order o(42, Side::Buy, 100, 50, 1);
        wal.log_new_order(o);
    }  // destructor flushes

    WalRecord buf[4];
    int count = read_wal(TMP, buf, 4);

    check(count == 1,                               "no_rec: expected 1 record");
    check(buf[0].rtype == WalRecordType::NewOrder,  "no_rec: wrong type");
    check(buf[0].side  == static_cast<uint8_t>(Side::Buy), "no_rec: wrong side");
    check(buf[0].id_a  == 42,                       "no_rec: wrong order_id");
    check(buf[0].price == 100,                      "no_rec: wrong price");
    check(buf[0].quantity == 50,                    "no_rec: wrong qty");
    check(buf[0].seq   == 1,                        "no_rec: seq must be 1");
}

static void test_cancel_record() {
    {
        WalLogger wal(TMP);
        wal.log_cancel(77);
    }

    WalRecord buf[4];
    int count = read_wal(TMP, buf, 4);

    check(count == 1,                             "cancel_rec: expected 1 record");
    check(buf[0].rtype == WalRecordType::Cancel,  "cancel_rec: wrong type");
    check(buf[0].id_a  == 77,                     "cancel_rec: wrong order_id");
}

static void test_modify_record() {
    {
        WalLogger wal(TMP);
        wal.log_modify(55, 105, 30);
    }

    WalRecord buf[4];
    int count = read_wal(TMP, buf, 4);

    check(count == 1,                             "mod_rec: expected 1 record");
    check(buf[0].rtype == WalRecordType::Modify,  "mod_rec: wrong type");
    check(buf[0].id_a  == 55,                     "mod_rec: wrong order_id");
    check(buf[0].price == 105,                    "mod_rec: wrong new price");
    check(buf[0].quantity == 30,                  "mod_rec: wrong new qty");
}

static void test_trade_record() {
    {
        WalLogger wal(TMP);
        Trade t(1, 10, 20, 100, 5, 99);
        wal.log_trade(t);
    }

    WalRecord buf[4];
    int count = read_wal(TMP, buf, 4);

    check(count == 1,                            "trade_rec: expected 1 record");
    check(buf[0].rtype == WalRecordType::Trade,  "trade_rec: wrong type");
    check(buf[0].id_a  == 10,                    "trade_rec: wrong maker_id");
    check(buf[0].id_b  == 20,                    "trade_rec: wrong taker_id");
    check(buf[0].price == 100,                   "trade_rec: wrong price");
    check(buf[0].quantity == 5,                  "trade_rec: wrong qty");
}

// ── Sequence numbers ──────────────────────────────────────────────────────────

static void test_seq_monotonic() {
    {
        WalLogger wal(TMP);
        wal.log_cancel(1);
        wal.log_cancel(2);
        wal.log_cancel(3);
    }

    WalRecord buf[8];
    int count = read_wal(TMP, buf, 8);

    check(count == 3,          "seq: expected 3 records");
    check(buf[0].seq == 1,     "seq: first seq = 1");
    check(buf[1].seq == 2,     "seq: second seq = 2");
    check(buf[2].seq == 3,     "seq: third seq = 3");
}

// ── Explicit flush before destructor ─────────────────────────────────────────

static void test_explicit_flush() {
    WalLogger wal(TMP);
    wal.log_cancel(99);
    wal.flush();

    // Read while the WalLogger is still alive (fd still open).
    WalRecord buf[4];
    int count = read_wal(TMP, buf, 4);

    check(count == 1,            "explicit_flush: record on disk after flush()");
    check(buf[0].id_a == 99,     "explicit_flush: correct order_id");
    check(wal.records_written() == 1, "explicit_flush: records_written = 1");
}

// ── Batch threshold triggers automatic flush ──────────────────────────────────

static void test_batch_threshold_flush() {
    {
        WalLogger wal(TMP);
        // Write exactly FLUSH_THRESHOLD records — should trigger an auto-flush.
        for (int i = 0; i < WalLogger::FLUSH_THRESHOLD; ++i)
            wal.log_cancel(static_cast<uint64_t>(i + 1));

        // At this point all records must be on disk (auto-flushed at threshold).
        WalRecord buf[1];
        // Just check we can read at least 1 record — the file was written.
        int count = read_wal(TMP, buf, 1);
        check(count == 1, "batch_flush: records on disk after threshold");
        check(wal.records_written() == WalLogger::FLUSH_THRESHOLD,
              "batch_flush: records_written == FLUSH_THRESHOLD");
    }

    // After destructor: all FLUSH_THRESHOLD records on disk (nothing extra to flush).
    WalRecord* all = new WalRecord[WalLogger::FLUSH_THRESHOLD + 1];
    int count = read_wal(TMP, all, WalLogger::FLUSH_THRESHOLD + 1);
    check(count == WalLogger::FLUSH_THRESHOLD, "batch_flush: final count correct");
    delete[] all;
}

// ── Mixed record types in sequence ───────────────────────────────────────────

static void test_mixed_sequence() {
    {
        WalLogger wal(TMP);
        Order o(1, Side::Sell, 200, 10, 0);
        wal.log_new_order(o);
        wal.log_cancel(1);
        Trade t(1, 1, 2, 200, 5, 0);
        wal.log_trade(t);
    }

    WalRecord buf[8];
    int count = read_wal(TMP, buf, 8);

    check(count == 3,                              "mixed: 3 records");
    check(buf[0].rtype == WalRecordType::NewOrder, "mixed: first=NewOrder");
    check(buf[1].rtype == WalRecordType::Cancel,   "mixed: second=Cancel");
    check(buf[2].rtype == WalRecordType::Trade,    "mixed: third=Trade");
    check(buf[0].seq == 1 && buf[1].seq == 2 && buf[2].seq == 3, "mixed: seq 1,2,3");
}

// ── Record size contract ──────────────────────────────────────────────────────

static void test_record_size() {
    check(sizeof(WalRecord) == 40, "record_size: WalRecord must be 40 bytes");
}

int main() {
    test_record_size();
    test_new_order_record();
    test_cancel_record();
    test_modify_record();
    test_trade_record();
    test_seq_monotonic();
    test_explicit_flush();
    test_batch_threshold_flush();
    test_mixed_sequence();

    ::unlink(TMP);
    std::puts("All WAL tests passed.");
    return 0;
}
