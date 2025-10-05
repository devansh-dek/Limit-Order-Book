#include "elob/logger.hpp"
#include "elob/event.hpp"
#include "elob/order.hpp"

#include <iostream>
#include <sstream>

namespace elob {

Logger::Logger(const std::string &path) {
    ofs_.open(path, std::ofstream::out | std::ofstream::trunc);
}

Logger::~Logger() {
    if (ofs_.is_open()) ofs_.flush();
}

void Logger::log_event(const Event &ev) {
    if (!ofs_.is_open()) return;

    // Format: E <event_id> <timestamp> <type> <payload...>
    std::visit([&](auto &p){
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, NewOrder>) {
            const Order &o = p.order;
            ofs_ << "E " << ev.event_id << ' ' << ev.timestamp << " NEWORDER "
                 << o.order_id << ' ' << side_chr(o.side) << ' ' << o.price << ' ' << o.quantity << ' ' << o.timestamp << '\n';
        } else if constexpr (std::is_same_v<T, Cancel>) {
            ofs_ << "E " << ev.event_id << ' ' << ev.timestamp << " CANCEL " << p.order_id << '\n';
        } else if constexpr (std::is_same_v<T, Modify>) {
            ofs_ << "E " << ev.event_id << ' ' << ev.timestamp << " MODIFY " << p.order_id << ' ' << p.new_price << ' ' << p.new_quantity << '\n';
        } else if constexpr (std::is_same_v<T, Trade>) {
            // logging an event that is already a trade (rare) — treat as trade line
            const Trade &t = p;
            ofs_ << "T " << t.trade_id << ' ' << t.timestamp << ' ' << t.maker_order_id << ' ' << t.taker_order_id << ' ' << t.price << ' ' << t.quantity << '\n';
        } else {
            // monostate: ignore
        }
    }, ev.payload);
    ofs_.flush();
}

void Logger::log_trade(const Trade &t) {
    if (!ofs_.is_open()) return;
    ofs_ << "T " << t.trade_id << ' ' << t.timestamp << ' ' << t.maker_order_id << ' ' << t.taker_order_id << ' ' << t.price << ' ' << t.quantity << '\n';
    ofs_.flush();
}

} // namespace elob
