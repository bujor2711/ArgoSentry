#include "VolkDMA/rate_limiter.hh"
#include <thread>

namespace VolkDMA {

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

    std::lock_guard<std::mutex> lock(mutex_);

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
            mutex_.unlock();
            std::this_thread::sleep_for(wait_time);
            mutex_.lock();

            // Reset after waiting
            bytes_consumed_.store(0, std::memory_order_relaxed);
            last_reset_ = std::chrono::steady_clock::now();

            // Update statistics
            total_waits_++;
            total_wait_time_ += std::chrono::duration_cast<std::chrono::milliseconds>(wait_time);
        } else {
            // Time window already passed, just reset
            bytes_consumed_.store(0, std::memory_order_relaxed);
            last_reset_ = std::chrono::steady_clock::now();
        }
    }

    // Consume bytes
    bytes_consumed_.fetch_add(bytes, std::memory_order_relaxed);
    total_bytes_consumed_ += bytes;
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
    std::lock_guard<std::mutex> lock(mutex_);
    return Stats{
        total_bytes_consumed_,
        total_waits_,
        total_wait_time_
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

} // namespace VolkDMA
