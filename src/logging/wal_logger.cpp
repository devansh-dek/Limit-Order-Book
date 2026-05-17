#include "logging/wal_logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace elob {

WalLogger::WalLogger(const char* path) {
    fd_ = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

WalLogger::~WalLogger() {
    flush();
    if (fd_ >= 0) ::close(fd_);
}

void WalLogger::push(WalRecord& r) {
    r.seq = ++seq_;
    buf_[buf_count_++] = r;
    if (buf_count_ == FLUSH_THRESHOLD) flush();
}

void WalLogger::flush() {
    if (fd_ < 0 || buf_count_ == 0) return;
    const char* ptr  = reinterpret_cast<const char*>(buf_);
    std::size_t left = static_cast<std::size_t>(buf_count_) * sizeof(WalRecord);
    while (left > 0) {
        ssize_t n = ::write(fd_, ptr, left);
        if (n <= 0) break;
        ptr  += n;
        left -= static_cast<std::size_t>(n);
    }
    total_written_ += static_cast<uint64_t>(buf_count_);
    buf_count_ = 0;
}

void WalLogger::log_new_order(const Order& o) {
    WalRecord r{};
    r.rtype    = WalRecordType::NewOrder;
    r.side     = static_cast<uint8_t>(o.side);
    r.id_a     = o.order_id;
    r.price    = o.price;
    r.quantity = o.quantity;
    push(r);
}

void WalLogger::log_cancel(uint64_t order_id) {
    WalRecord r{};
    r.rtype = WalRecordType::Cancel;
    r.id_a  = order_id;
    push(r);
}

void WalLogger::log_modify(uint64_t order_id, int64_t new_price, uint64_t new_qty) {
    WalRecord r{};
    r.rtype    = WalRecordType::Modify;
    r.id_a     = order_id;
    r.price    = new_price;
    r.quantity = new_qty;
    push(r);
}

void WalLogger::log_trade(const Trade& t) {
    WalRecord r{};
    r.rtype    = WalRecordType::Trade;
    r.id_a     = t.maker_order_id;
    r.id_b     = t.taker_order_id;
    r.price    = t.price;
    r.quantity = t.quantity;
    push(r);
}

} // namespace elob
