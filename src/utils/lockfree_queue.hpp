// Lock-free SPSC (Single Producer Single Consumer) queue for EfficientLimitOrderBook
// Based on ring buffer with atomic operations
#pragma once

#include <atomic>
#include <memory>
#include <optional>

namespace elob {

// Lock-free SPSC queue using ring buffer with power-of-2 size
// Thread-safe for one producer and one consumer
template <typename T, size_t Capacity = 1024>
class LockFreeQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    LockFreeQueue() : head_(0), tail_(0) {
        // Initialize storage
        for (size_t i = 0; i < Capacity; ++i) {
            storage_[i] = std::nullopt;
        }
    }

    // Producer side: enqueue item
    // Returns true if enqueued, false if queue is full
    bool push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = increment(current_tail);

        // Check if queue is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        // Write item
        storage_[current_tail] = item;

        // Publish the write
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Consumer side: dequeue item
    // Returns std::nullopt if queue is empty
    std::optional<T> pop() {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Check if queue is empty
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        // Read item
        std::optional<T> item = storage_[current_head];
        storage_[current_head] = std::nullopt;  // Clear slot

        // Publish the read
        head_.store(increment(current_head), std::memory_order_release);
        return item;
    }

    // Check if queue is empty (approximate, for monitoring only)
    bool empty() const {
        return head_.load(std::memory_order_relaxed) ==
               tail_.load(std::memory_order_relaxed);
    }

    // Get approximate size (for monitoring only)
    size_t size() const {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t t = tail_.load(std::memory_order_relaxed);
        return (t >= h) ? (t - h) : (Capacity - h + t);
    }

    // Get capacity
    constexpr size_t capacity() const { return Capacity; }

private:
    static constexpr size_t increment(size_t idx) {
        return (idx + 1) & (Capacity - 1);  // Fast modulo for power-of-2
    }

    // Padding to prevent false sharing between head and tail
    alignas(64) std::atomic<size_t> head_;  // Consumer index
    char padding1_[64 - sizeof(std::atomic<size_t>)];

    alignas(64) std::atomic<size_t> tail_;  // Producer index
    char padding2_[64 - sizeof(std::atomic<size_t>)];

    // Ring buffer storage
    alignas(64) std::optional<T> storage_[Capacity];
};

}  // namespace elob
