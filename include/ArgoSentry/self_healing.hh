// ArgoSentry Self-Healing System v3.0
// Automatic recovery from DMA failures with retry policies
// Part of Phase 3: Health Monitoring System

#pragma once

#include <functional>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <system_error>
#include <mutex>
#include <optional>

namespace ArgoSentry {

// Forward declarations
class CircuitBreaker;
enum class CircuitState;

/**
 * @brief Retry policy types for self-healing operations
 * @since v3.0
 */
enum class RetryPolicy {
    NONE,              ///< No retries (fail immediately)
    FIXED,             ///< Fixed delay between retries
    LINEAR,            ///< Linear backoff (delay increases linearly)
    EXPONENTIAL,       ///< Exponential backoff (recommended for most cases)
    FIBONACCI          ///< Fibonacci backoff (aggressive increase)
};

/**
 * @brief Configuration for self-healing behavior
 * @since v3.0
 */
struct SelfHealingConfig {
    // Retry configuration
    RetryPolicy retry_policy{RetryPolicy::EXPONENTIAL};
    size_t max_retry_attempts{3};
    std::chrono::milliseconds initial_retry_delay{100};
    std::chrono::milliseconds max_retry_delay{5000};
    double backoff_multiplier{2.0};

    // Circuit breaker integration
    bool use_circuit_breaker{true};
    bool auto_reset_on_success{true};

    // Reconnection configuration
    bool auto_reconnect{true};
    std::chrono::seconds reconnect_timeout{30};
    size_t max_reconnect_attempts{5};

    // Fallback configuration
    bool enable_fallback{true};
    std::function<void(const std::string&)> fallback_handler;

    // Health check configuration
    bool enable_health_checks{true};
    std::chrono::seconds health_check_interval{10};
    size_t health_check_failures_before_reconnect{3};

    // Callbacks
    std::function<void(const std::string& operation, size_t attempt, const std::error_code& error)> 
        on_retry_attempt;
    std::function<void(const std::string& operation, size_t total_attempts)> 
        on_retry_exhausted;
    std::function<void()> 
        on_reconnect_start;
    std::function<void(bool success)> 
        on_reconnect_complete;
};

/**
 * @brief Statistics for self-healing operations
 * @since v3.0
 */
struct SelfHealingStats {
    // Retry statistics
    size_t total_retry_attempts{0};
    size_t successful_retries{0};
    size_t failed_retries{0};
    size_t retry_exhausted_count{0};

    // Reconnection statistics
    size_t reconnection_attempts{0};
    size_t successful_reconnections{0};
    size_t failed_reconnections{0};
    std::chrono::system_clock::time_point last_reconnection_time;

    // Fallback statistics
    size_t fallback_invocations{0};
    std::chrono::system_clock::time_point last_fallback_time;

    // Health check statistics
    size_t total_health_checks{0};
    size_t failed_health_checks{0};
    size_t consecutive_health_failures{0};
    std::chrono::system_clock::time_point last_health_check_time;

    // Timing statistics
    std::chrono::milliseconds total_retry_time{0};
    std::chrono::milliseconds average_retry_delay{0};

    /**
     * @brief Calculate retry success rate (0.0 - 100.0)
     */
    [[nodiscard]] double get_retry_success_rate() const {
        if (total_retry_attempts == 0) return 100.0;
        return (successful_retries * 100.0) / total_retry_attempts;
    }

    /**
     * @brief Calculate reconnection success rate (0.0 - 100.0)
     */
    [[nodiscard]] double get_reconnection_success_rate() const {
        if (reconnection_attempts == 0) return 100.0;
        return (successful_reconnections * 100.0) / reconnection_attempts;
    }

    /**
     * @brief Calculate health check success rate (0.0 - 100.0)
     */
    [[nodiscard]] double get_health_check_success_rate() const {
        if (total_health_checks == 0) return 100.0;
        return ((total_health_checks - failed_health_checks) * 100.0) / total_health_checks;
    }
};

/**
 * @brief Self-healing system for automatic DMA recovery
 * 
 * Provides automatic recovery from failures through:
 * - Intelligent retry policies (exponential backoff, etc.)
 * - Automatic reconnection to DMA device
 * - Circuit breaker integration for failure detection
 * - Fallback strategies when recovery fails
 * - Health monitoring and proactive recovery
 * 
 * @since v3.0
 * @thread_safety All public methods are thread-safe
 * 
 * @example
 * @code
 * SelfHealingConfig config;
 * config.retry_policy = RetryPolicy::EXPONENTIAL;
 * config.max_retry_attempts = 5;
 * config.auto_reconnect = true;
 * 
 * SelfHealing healer(config, circuit_breaker);
 * 
 * // Execute operation with automatic retry
 * auto result = healer.execute_with_retry([&]() {
 *     return dma.read<uint64_t>(address, pid);
 * }, "memory_read");
 * 
 * if (result) {
 *     std::cout << "Value: 0x" << std::hex << *result << "\n";
 * }
 * @endcode
 */
class SelfHealing {
public:
    /**
     * @brief Construct self-healing system
     * @param config Self-healing configuration
     * @param circuit_breaker Optional circuit breaker for integration (can be nullptr)
     */
    explicit SelfHealing(
        SelfHealingConfig config = {},
        CircuitBreaker* circuit_breaker = nullptr
    );

    /**
     * @brief Destructor
     */
    ~SelfHealing();

    // Delete copy constructor and assignment
    SelfHealing(const SelfHealing&) = delete;
    SelfHealing& operator=(const SelfHealing&) = delete;

    // Allow move
    SelfHealing(SelfHealing&&) noexcept = default;
    SelfHealing& operator=(SelfHealing&&) noexcept = default;

    /**
     * @brief Execute operation with automatic retry on failure
     * @tparam Func Callable that returns std::error_code or void
     * @param operation The operation to execute
     * @param operation_name Human-readable name for logging
     * @return Error code (success if operation succeeded)
     * 
     * @example
     * @code
     * auto error = healer.execute_with_retry([&]() {
     *     return dma.find_signature(pattern, start, end, pid);
     * }, "signature_scan");
     * @endcode
     */
    template<typename Func>
    [[nodiscard]] std::error_code execute_with_retry(
        Func&& operation,
        const std::string& operation_name = "operation"
    );

    /**
     * @brief Execute operation with automatic retry and return result
     * @tparam Func Callable that returns a value
     * @param operation The operation to execute
     * @param operation_name Human-readable name for logging
     * @return Optional result (nullopt if all retries failed)
     * 
     * @example
     * @code
     * auto result = healer.execute_with_retry_result<uint64_t>([&]() {
     *     return dma.read<uint64_t>(address, pid);
     * }, "memory_read");
     * @endcode
     */
    template<typename Result, typename Func>
    [[nodiscard]] std::optional<Result> execute_with_retry_result(
        Func&& operation,
        const std::string& operation_name = "operation"
    );

    /**
     * @brief Attempt to reconnect to DMA device
     * @param reconnect_func Function that performs the reconnection
     * @return true if reconnection succeeded
     * 
     * @example
     * @code
     * bool success = healer.attempt_reconnect([&]() {
     *     return dma.reinitialize();
     * });
     * @endcode
     */
    [[nodiscard]] bool attempt_reconnect(
        std::function<bool()> reconnect_func
    );

    /**
     * @brief Perform health check
     * @param health_check_func Function that performs the health check
     * @return true if health check passed
     * 
     * @example
     * @code
     * bool healthy = healer.perform_health_check([&]() {
     *     return dma.is_connected();
     * });
     * @endcode
     */
    [[nodiscard]] bool perform_health_check(
        std::function<bool()> health_check_func
    );

    /**
     * @brief Get current self-healing statistics
     * @return Copy of current statistics
     */
    [[nodiscard]] SelfHealingStats get_stats() const;

    /**
     * @brief Reset all statistics
     */
    void reset_stats();

    /**
     * @brief Get current configuration
     * @return Copy of current configuration
     */
    [[nodiscard]] SelfHealingConfig get_config() const;

    /**
     * @brief Update configuration at runtime
     * @param new_config New configuration to apply
     * 
     * @note Thread-safe, changes take effect immediately
     */
    void update_config(const SelfHealingConfig& new_config);

    /**
     * @brief Set circuit breaker instance
     * @param cb Circuit breaker pointer (can be nullptr to disable)
     */
    void set_circuit_breaker(CircuitBreaker* cb) noexcept;

    /**
     * @brief Get circuit breaker instance
     * @return Pointer to circuit breaker (can be nullptr)
     */
    [[nodiscard]] CircuitBreaker* get_circuit_breaker() const noexcept;

private:
    /**
     * @brief Calculate delay for next retry attempt
     * @param attempt Current attempt number (0-based)
     * @return Delay duration in milliseconds
     */
    [[nodiscard]] std::chrono::milliseconds calculate_retry_delay(size_t attempt) const;

    /**
     * @brief Check if operation should be retried
     * @param attempt Current attempt number
     * @param error Error from last attempt
     * @return true if should retry
     */
    [[nodiscard]] bool should_retry(size_t attempt, const std::error_code& error) const;

    /**
     * @brief Invoke fallback handler if configured
     * @param operation_name Name of the failed operation
     */
    void invoke_fallback(const std::string& operation_name);

    /**
     * @brief Update retry statistics
     * @param success Whether retry succeeded
     * @param delay Delay duration
     */
    void update_retry_stats(bool success, std::chrono::milliseconds delay);

    // Configuration
    SelfHealingConfig config_;

    // Circuit breaker integration
    CircuitBreaker* circuit_breaker_;

    // Statistics
    mutable std::mutex mutex_;
    SelfHealingStats stats_;
};

/**
 * @brief Convert RetryPolicy to string
 */
inline const char* to_string(RetryPolicy policy) {
    switch (policy) {
        case RetryPolicy::NONE:        return "NONE";
        case RetryPolicy::FIXED:       return "FIXED";
        case RetryPolicy::LINEAR:      return "LINEAR";
        case RetryPolicy::EXPONENTIAL: return "EXPONENTIAL";
        case RetryPolicy::FIBONACCI:   return "FIBONACCI";
        default:                       return "UNKNOWN";
    }
}

} // namespace ArgoSentry
