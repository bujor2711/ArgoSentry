#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <cstddef>

namespace ArgoSentry {

/**
 * @brief Thread-safe rate limiter for DMA operations
 * 
 * Implements token bucket algorithm to limit bytes per second.
 * Designed for multi-threaded environments with minimal overhead.
 * 
 * Example usage:
 * @code
 * RateLimiter limiter(1024 * 1024);  // 1 MB/s limit
 * 
 * // Before each read:
 * limiter.wait_if_needed(buffer_size);
 * // Perform read...
 * @endcode
 * 
 * @since v2.3
 */
class RateLimiter {
public:
    /**
     * @brief Construct rate limiter with specified limit
     * @param bytes_per_sec Maximum bytes per second (0 = unlimited)
     */
    explicit RateLimiter(size_t bytes_per_sec = 0);

    /**
     * @brief Destructor
     */
    ~RateLimiter() = default;

    // Non-copyable
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    // Movable
    RateLimiter(RateLimiter&&) noexcept = default;
    RateLimiter& operator=(RateLimiter&&) noexcept = default;

    /**
     * @brief Wait if needed to stay within rate limit
     * @param bytes Number of bytes about to be consumed
     * 
     * Thread-safe: Can be called concurrently from multiple threads.
     * If rate limit would be exceeded, blocks until next time window.
     */
    void wait_if_needed(size_t bytes);

    /**
     * @brief Check if rate limiting is enabled
     * @return True if rate limiting is active
     */
    [[nodiscard]] bool is_enabled() const noexcept;

    /**
     * @brief Get current bytes consumed in current window
     * @return Bytes consumed since last reset
     */
    [[nodiscard]] size_t get_current_usage() const noexcept;

    /**
     * @brief Get configured rate limit
     * @return Bytes per second limit (0 = unlimited)
     */
    [[nodiscard]] size_t get_limit() const noexcept;

    /**
     * @brief Update rate limit
     * @param bytes_per_sec New limit (0 = unlimited)
     */
    void set_limit(size_t bytes_per_sec) noexcept;

    /**
     * @brief Reset rate limiter state
     * 
     * Clears current consumption counter and resets time window.
     * Thread-safe.
     */
    void reset();

    /**
     * @brief Get statistics about rate limiting
     */
    struct Stats {
        size_t total_bytes_consumed{0};    ///< Total bytes ever consumed
        size_t total_waits{0};              ///< Number of times throttled
        std::chrono::milliseconds total_wait_time{0};  ///< Total time spent waiting
    };

    /**
     * @brief Get rate limiter statistics
     * @return Current statistics
     */
    [[nodiscard]] Stats get_stats() const;

private:
    mutable std::mutex mutex_;                          ///< Protects state
    std::atomic<size_t> bytes_consumed_{0};             ///< Current window consumption
    std::chrono::steady_clock::time_point last_reset_;  ///< Last window reset time
    size_t bytes_per_second_limit_;                     ///< Rate limit (0 = unlimited)

    // ✅ Statistics - now atomic for thread-safe access
    std::atomic<size_t> total_bytes_consumed_{0};       ///< Total bytes consumed (all-time)
    std::atomic<size_t> total_waits_{0};                ///< Total number of waits
    std::atomic<uint64_t> total_wait_time_ms_{0};       ///< Total wait time in milliseconds

    /**
     * @brief Check and reset window if needed
     * @return True if window was reset
     * 
     * Must be called with mutex_ held.
     */
    bool check_and_reset_window();
};

} // namespace ArgoSentry
