// Simple metrics collector for the engine
#pragma once

#include <cstdint>
#include <string>

namespace elob {

struct Metrics {
    uint64_t orders_ingested = 0;
    uint64_t trades_executed = 0;
    uint64_t cancels = 0;
    uint64_t modifies = 0;

    void reset() noexcept {
        orders_ingested = trades_executed = cancels = modifies = 0;
    }

    std::string summary() const;
};

} // namespace elob
