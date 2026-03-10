// Multi-producer single-consumer engine wrapper for EfficientLimitOrderBook
// Uses a mutex + condition_variable protected deque for MPSC semantics
#pragma once

#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"
#include "engine/trade.hpp"
#include "data/event.hpp"

#include <atomic>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>

namespace elob {

// Simple MPSC engine: multiple producers may call submit(), single
// background consumer thread processes events in FIFO order.
class EngineMPSC {
public:
    EngineMPSC(size_t capacity = 16384)
        : book_()
        , engine_(book_)
        , ingestor_(book_, engine_)
        , running_(false)
        , capacity_(capacity)
        , processed_count_(0)
    {}

    ~EngineMPSC() { stop(); }

    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return; // already running
        }

        worker_thread_ = std::thread(&EngineMPSC::worker_loop, this);
    }

    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
            return; // not running
        }

        cv_.notify_all();
        if (worker_thread_.joinable()) worker_thread_.join();
    }

    // Try to submit an event. Returns false if queue is at capacity.
    bool submit(const Event& ev) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (queue_.size() >= capacity_) return false;
        queue_.push_back(ev);
        lock.unlock();
        cv_.notify_one();
        return true;
    }

    size_t queue_size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    uint64_t processed_count() const { return processed_count_.load(std::memory_order_relaxed); }

    // Wait until queue is drained
    void drain() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                if (queue_.empty() && !processing_.load(std::memory_order_acquire)) break;
            }
            std::this_thread::yield();
        }
    }

    const OrderBook& book() const { return book_; }

private:
    void worker_loop() {
        while (running_.load(std::memory_order_acquire)) {
            Event ev;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this] { return !queue_.empty() || !running_.load(std::memory_order_acquire); });

                if (!running_.load(std::memory_order_acquire) && queue_.empty()) return;

                ev = queue_.front();
                queue_.pop_front();
                processing_.store(true, std::memory_order_release);
            }

            // Process event outside lock
            ingestor_.process(ev);
            processed_count_.fetch_add(1, std::memory_order_relaxed);
            processing_.store(false, std::memory_order_release);
        }

        // Drain any remaining events before exiting
        while (true) {
            Event ev;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                if (queue_.empty()) break;
                ev = queue_.front();
                queue_.pop_front();
                processing_.store(true, std::memory_order_release);
            }
            ingestor_.process(ev);
            processed_count_.fetch_add(1, std::memory_order_relaxed);
            processing_.store(false, std::memory_order_release);
        }
    }

    OrderBook book_;
    MatchingEngine engine_;
    EventIngestor ingestor_;

    // Queue and synchronization
    std::deque<Event> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    const size_t capacity_;

    // Control flags
    std::atomic<bool> running_;
    std::atomic<bool> processing_{false};
    std::atomic<uint64_t> processed_count_;

    // Worker thread
    std::thread worker_thread_;
};

} // namespace elob
