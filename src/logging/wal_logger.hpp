#pragma once

#include "engine/order.hpp"
#include "engine/trade.hpp"

#include <cstdint>

namespace elob {

enum class WalRecordType : uint8_t {
    NewOrder = 1,
    Cancel   = 2,
    Modify   = 3,
    Trade    = 4,
};

// Fixed-size 40-byte record. All fields are little-endian native.
// Field reuse by type:
//   NewOrder : id_a=order_id, id_b=0,           price=price, quantity=qty
//   Cancel   : id_a=order_id, id_b=0,           price=0,     quantity=0
//   Modify   : id_a=order_id, id_b=0,           price=new_price, quantity=new_qty
//   Trade    : id_a=maker_id, id_b=taker_id,    price=price, quantity=qty
#pragma pack(push, 1)
struct WalRecord {
    WalRecordType rtype;
    uint8_t       side;      // Side enum for NewOrder; 0 otherwise
    uint16_t      reserved;
    uint32_t      seq;       // monotonic within this WAL file
    uint64_t      id_a;
    uint64_t      id_b;
    int64_t       price;
    uint64_t      quantity;
};
#pragma pack(pop)
static_assert(sizeof(WalRecord) == 40, "WalRecord must be 40 bytes");

// Binary append-only WAL.
// Records are buffered and written in batches for throughput.
// Contrast with Logger (text, per-event flush): that approach saturates at ~50K events/s
// due to syscall + OS page-cache flush overhead. Batch binary writes amortize that cost
// across FLUSH_THRESHOLD records, reaching millions of events/s on NVMe.
class WalLogger {
public:
    static constexpr int FLUSH_THRESHOLD = 4096;  // ~160 KB per batch write

    // Opens (or creates) the WAL file at path. Truncates on open.
    explicit WalLogger(const char* path);
    ~WalLogger();

    void log_new_order(const Order& o);
    void log_cancel(uint64_t order_id);
    void log_modify(uint64_t order_id, int64_t new_price, uint64_t new_qty);
    void log_trade(const Trade& t);

    // Force all buffered records to disk.
    void flush();

    uint64_t records_written()  const noexcept { return total_written_; }
    uint32_t next_seq()         const noexcept { return seq_; }
    bool     is_open()          const noexcept { return fd_ >= 0; }

private:
    void push(WalRecord& r);  // append to buffer, flush if threshold hit

    int       fd_            = -1;
    WalRecord buf_[FLUSH_THRESHOLD];
    int       buf_count_     = 0;
    uint32_t  seq_           = 0;
    uint64_t  total_written_ = 0;
};

} // namespace elob
