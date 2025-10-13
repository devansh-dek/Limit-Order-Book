// Simple deterministic logger for events and trades
#pragma once

#include "data/event.hpp"
#include "engine/trade.hpp"

#include <string>
#include <fstream>
#include <optional>

namespace elob {

class Logger {
public:
    explicit Logger(const std::string &path);
    ~Logger();

    void log_event(const Event &ev);
    void log_trade(const Trade &t);

private:
    std::ofstream ofs_;

    // helpers
    static const char* side_chr(Side s) noexcept { return (s == Side::Buy) ? "B" : "S"; }
};

} // namespace elob
