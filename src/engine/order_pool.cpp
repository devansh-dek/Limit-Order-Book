#include "engine/order_pool.hpp"

namespace elob {

OrderPool::OrderPool(int32_t initial_capacity) {
    pool_.resize(static_cast<size_t>(initial_capacity));
    free_list_.reserve(static_cast<size_t>(initial_capacity));
    for (int32_t i = initial_capacity - 1; i >= 0; --i)
        free_list_.push_back(i);
}

int32_t OrderPool::alloc(const Order& o) {
    if (free_list_.empty()) {
        int32_t idx = static_cast<int32_t>(pool_.size());
        pool_.push_back({o, -1, -1});
        return idx;
    }
    int32_t idx = free_list_.back();
    free_list_.pop_back();
    pool_[static_cast<size_t>(idx)] = {o, -1, -1};
    return idx;
}

void OrderPool::free(int32_t idx) {
    free_list_.push_back(idx);
}

} // namespace elob
