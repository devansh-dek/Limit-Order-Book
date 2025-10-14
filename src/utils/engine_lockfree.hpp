// Lock-free concurrent engine wrapper for EfficientLimitOrderBook
// Uses lock-free SPSC queue for event processing
#pragma once

#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "engine/trade.hpp"
#include "data/event.hpp"
#include "utils/lockfree_queue.hpp"

#include <atomic>
#include <thread>
#include <vector>

namespace elob {

// Lock-free engine using SPSC queue for single producer, single consumer
// More scalable than mutex-based approach for this pattern
class EngineLockFree {
public:
    EngineLockFree() 
        : book_()
        , engine_(book_)
        , ingestor_(book_, engine_)
        , running_(false)
        , worker_thread_()
    {}

    ~EngineLockFree() {
        stop();
    }

    // Start background worker thread (consumer)
    void start() {
        if (running_.load(std::memory_order_acquire)) {
            return;  // Already running
        }

        running_.store(true, std::memory_order_release);
        worker_thread_ = std::thread(&EngineLockFree::worker_loop, this);
    }

    // Stop background worker thread
    void stop() {
        if (!running_.load(std::memory_order_acquire)) {
            return;  // Not running
        }

        running_.store(false, std::memory_order_release);
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    // Producer side: enqueue event for processing
    // Returns true if enqueued, false if queue is full
    bool submit(const Event& event) {
        return event_queue_.push(event);
    }

    // Get approximate queue size (for monitoring)
    size_t queue_size() const {
        return event_queue_.size();
    }

    // Get total processed count
    uint64_t processed_count() const {
        return processed_count_.load(std::memory_order_relaxed);
    }

    // Wait until queue is drained (for testing/benchmarking)
    void drain() {
        while (!event_queue_.empty() || processing_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    // Direct access to book for queries (not thread-safe with worker!)
    const OrderBook& book() const { return book_; }

private:
    // Worker thread loop (consumer)
    void worker_loop() {
        while (running_.load(std::memory_order_acquire)) {
            auto maybe_event = event_queue_.pop();
            
            if (maybe_event.has_value()) {
                processing_.store(true, std::memory_order_release);
                
                // Process event
                ingestor_.process(maybe_event.value());
                processed_count_.fetch_add(1, std::memory_order_relaxed);
                
                processing_.store(false, std::memory_order_release);
            } else {
                // Queue empty, yield to avoid busy-wait
                std::this_thread::yield();
            }
        }
    }

    OrderBook book_;
    MatchingEngine engine_;
    EventIngestor ingestor_;

    // Lock-free queue (SPSC)
    static constexpr size_t QUEUE_CAPACITY = 8192;
    LockFreeQueue<Event, QUEUE_CAPACITY> event_queue_;

    // Control flags
    std::atomic<bool> running_;
    std::atomic<bool> processing_;  // True while processing an event
    std::atomic<uint64_t> processed_count_;

    // Worker thread
    std::thread worker_thread_;
};

}  // namespace elob
