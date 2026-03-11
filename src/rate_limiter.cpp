#include "ArgoSentry/rate_limiter.hh"
#include <thread>

namespace ArgoSentry {

RateLimiter::RateLimiter(size_t bytes_per_sec)
    : bytes_per_second_limit_(bytes_per_sec)
    , last_reset_(std::chrono::steady_clock::now())
{
}

void RateLimiter::wait_if_needed(size_t bytes) {
    // Fast path: No rate limiting enabled
    if (bytes_per_second_limit_ == 0) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);  // ✅ Use unique_lock for manual control

    // Check if we need to reset the time window
    check_and_reset_window();

    // Get current consumption
    size_t current = bytes_consumed_.load(std::memory_order_relaxed);

    // Check if we would exceed the limit
    if (current + bytes > bytes_per_second_limit_) {
        // Calculate how long to wait
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_reset_
        );

        auto wait_time = std::chrono::seconds(1) - elapsed;

        // Only wait if we're still in the same second
        if (wait_time.count() > 0) {
            // Release mutex during sleep to avoid blocking other threads
            lock.unlock();  // ✅ Safe manual unlock with unique_lock
            std::this_thread::sleep_for(wait_time);
            lock.lock();    // ✅ Safe manual relock with unique_lock

            // Reset after waiting
            bytes_consumed_.store(0, std::memory_order_relaxed);
            last_reset_ = std::chrono::steady_clock::now();

            // ✅ Update atomic statistics (thread-safe)
            total_waits_.fetch_add(1, std::memory_order_relaxed);
            total_wait_time_ms_.fetch_add(
                std::chrono::duration_cast<std::chrono::milliseconds>(wait_time).count(),
                std::memory_order_relaxed
            );
        } else {
            // Time window already passed, just reset
            bytes_consumed_.store(0, std::memory_order_relaxed);
            last_reset_ = std::chrono::steady_clock::now();
        }
    }

    // Consume bytes
    bytes_consumed_.fetch_add(bytes, std::memory_order_relaxed);
    total_bytes_consumed_.fetch_add(bytes, std::memory_order_relaxed);  // ✅ Atomic increment
}

bool RateLimiter::is_enabled() const noexcept {
    return bytes_per_second_limit_ > 0;
}

size_t RateLimiter::get_current_usage() const noexcept {
    return bytes_consumed_.load(std::memory_order_relaxed);
}

size_t RateLimiter::get_limit() const noexcept {
    return bytes_per_second_limit_;
}

void RateLimiter::set_limit(size_t bytes_per_sec) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    bytes_per_second_limit_ = bytes_per_sec;
    
    // Reset state when changing limit
    bytes_consumed_.store(0, std::memory_order_relaxed);
    last_reset_ = std::chrono::steady_clock::now();
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    bytes_consumed_.store(0, std::memory_order_relaxed);
    last_reset_ = std::chrono::steady_clock::now();
}

RateLimiter::Stats RateLimiter::get_stats() const {
    // ✅ Read atomic statistics (no mutex needed - lock-free)
    return Stats{
        total_bytes_consumed_.load(std::memory_order_relaxed),
        total_waits_.load(std::memory_order_relaxed),
        std::chrono::milliseconds(total_wait_time_ms_.load(std::memory_order_relaxed))
    };
}

bool RateLimiter::check_and_reset_window() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_reset_
    ).count();

    // Reset counter every second
    if (elapsed >= 1) {
        bytes_consumed_.store(0, std::memory_order_relaxed);
        last_reset_ = now;
        return true;
    }

    return false;
}

} // namespace ArgoSentry
