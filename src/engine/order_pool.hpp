#pragma once

#include "engine/order.hpp"

#include <vector>
#include <cstdint>

namespace elob {

struct PooledOrder {
    Order   data;
    int32_t prev = -1;
    int32_t next = -1;
};

// Fixed-slab order pool. Allocates from a pre-reserved flat array;
// recycles freed slots via a free-list. Zero heap allocation on the hot path
// after warm-up.
class OrderPool {
public:
    explicit OrderPool(int32_t initial_capacity = 65536);

    int32_t alloc(const Order& o);
    void    free(int32_t idx);

    PooledOrder&       at(int32_t idx)       { return pool_[static_cast<size_t>(idx)]; }
    const PooledOrder& at(int32_t idx) const { return pool_[static_cast<size_t>(idx)]; }

private:
    std::vector<PooledOrder> pool_;
    std::vector<int32_t>     free_list_;
};

} // namespace elob
