// ArgoSentry Self-Healing System Implementation v3.0

#include "ArgoSentry/self_healing.hh"
#include "ArgoSentry/circuit_breaker.hh"

#include <algorithm>
#include <thread>
#include <cmath>
#include <mutex>

namespace ArgoSentry {

// ============================================================================
// Constructor / Destructor
// ============================================================================

SelfHealing::SelfHealing(
    SelfHealingConfig config,
    CircuitBreaker* circuit_breaker
)
    : config_(std::move(config))
    , circuit_breaker_(circuit_breaker)
    , mutex_()
    , stats_()
{
    // Initialize timestamps
    stats_.last_reconnection_time = std::chrono::system_clock::now();
    stats_.last_fallback_time = std::chrono::system_clock::now();
    stats_.last_health_check_time = std::chrono::system_clock::now();
}

SelfHealing::~SelfHealing() = default;

// ============================================================================
// Retry Logic
// ============================================================================

std::chrono::milliseconds SelfHealing::calculate_retry_delay(size_t attempt) const {
    if (config_.retry_policy == RetryPolicy::NONE) {
        return std::chrono::milliseconds(0);
    }

    auto base_delay = config_.initial_retry_delay.count();
    int64_t delay_ms = 0;

    switch (config_.retry_policy) {
        case RetryPolicy::FIXED:
            delay_ms = base_delay;
            break;

        case RetryPolicy::LINEAR:
            delay_ms = base_delay * (attempt + 1);
            break;

        case RetryPolicy::EXPONENTIAL:
            delay_ms = static_cast<int64_t>(
                base_delay * std::pow(config_.backoff_multiplier, attempt)
            );
            break;

        case RetryPolicy::FIBONACCI: {
            // Fibonacci sequence: 1, 1, 2, 3, 5, 8, 13, 21, ...
            size_t fib_prev = 1;
            size_t fib_curr = 1;
            for (size_t i = 0; i < attempt; ++i) {
                size_t fib_next = fib_prev + fib_curr;
                fib_prev = fib_curr;
                fib_curr = fib_next;
            }
            delay_ms = base_delay * fib_curr;
            break;
        }

        default:
            delay_ms = base_delay;
            break;
    }

    // Clamp to max delay
    auto max_delay = config_.max_retry_delay.count();
    if (delay_ms > max_delay) {
        delay_ms = max_delay;
    }

    return std::chrono::milliseconds(delay_ms);
}

bool SelfHealing::should_retry(size_t attempt, const std::error_code& error) const {
    // Check retry policy
    if (config_.retry_policy == RetryPolicy::NONE) {
        return false;
    }

    // Check max attempts
    if (attempt >= config_.max_retry_attempts) {
        return false;
    }

    // Check circuit breaker state
    if (config_.use_circuit_breaker && circuit_breaker_) {
        auto state = circuit_breaker_->get_state();
        if (state == CircuitState::OPEN) {
            return false;  // Don't retry if circuit is open
        }
    }

    // Always retry on error (unless circuit is open or max attempts reached)
    return error.value() != 0;
}

template<typename Func>
std::error_code SelfHealing::execute_with_retry(
    Func&& operation,
    const std::string& operation_name
) {
    std::error_code last_error;
    size_t attempt = 0;

    for (; attempt < config_.max_retry_attempts; ++attempt) {
        // Execute operation
        try {
            // If circuit breaker is enabled, use it
            if (config_.use_circuit_breaker && circuit_breaker_) {
                last_error = circuit_breaker_->execute(std::forward<Func>(operation));
            } else {
                last_error = operation();
            }

            // Success!
            if (!last_error) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.successful_retries++;
                    stats_.total_retry_attempts += attempt;
                }

                // Auto-reset circuit breaker on success if configured
                if (config_.auto_reset_on_success && circuit_breaker_) {
                    circuit_breaker_->reset();
                }

                return last_error;
            }

            // Operation failed, check if we should retry
            if (!should_retry(attempt, last_error)) {
                break;
            }

            // Calculate retry delay
            auto delay = calculate_retry_delay(attempt);

            // Invoke retry callback if configured
            if (config_.on_retry_attempt) {
                config_.on_retry_attempt(operation_name, attempt + 1, last_error);
            }

            // Update statistics
            update_retry_stats(false, delay);

            // Wait before retry
            std::this_thread::sleep_for(delay);

        } catch (const std::exception&) {
            // Exception thrown, treat as error
            last_error = std::make_error_code(std::errc::operation_canceled);
            break;
        }
    }

    // All retries exhausted
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.failed_retries++;
        stats_.retry_exhausted_count++;
        stats_.total_retry_attempts += attempt;
    }

    // Invoke retry exhausted callback
    if (config_.on_retry_exhausted) {
        config_.on_retry_exhausted(operation_name, attempt);
    }

    // Invoke fallback if configured
    if (config_.enable_fallback) {
        invoke_fallback(operation_name);
    }

    return last_error;
}

template<typename Result, typename Func>
std::optional<Result> SelfHealing::execute_with_retry_result(
    Func&& operation,
    const std::string& operation_name
) {
    std::optional<Result> last_result;
    size_t attempt = 0;

    for (; attempt < config_.max_retry_attempts; ++attempt) {
        try {
            // Execute operation
            last_result = operation();

            // Success!
            if (last_result.has_value()) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.successful_retries++;
                    stats_.total_retry_attempts += attempt;
                }

                // Auto-reset circuit breaker on success
                if (config_.auto_reset_on_success && circuit_breaker_) {
                    circuit_breaker_->reset();
                }

                return last_result;
            }

            // Operation failed (returned nullopt)
            std::error_code error(1, std::generic_category());
            if (!should_retry(attempt, error)) {
                break;
            }

            // Calculate retry delay
            auto delay = calculate_retry_delay(attempt);

            // Invoke retry callback
            if (config_.on_retry_attempt) {
                config_.on_retry_attempt(operation_name, attempt + 1, error);
            }

            // Update statistics
            update_retry_stats(false, delay);

            // Wait before retry
            std::this_thread::sleep_for(delay);

        } catch (const std::exception&) {
            break;
        }
    }

    // All retries exhausted
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.failed_retries++;
        stats_.retry_exhausted_count++;
        stats_.total_retry_attempts += attempt;
    }

    // Invoke callbacks
    if (config_.on_retry_exhausted) {
        config_.on_retry_exhausted(operation_name, attempt);
    }

    if (config_.enable_fallback) {
        invoke_fallback(operation_name);
    }

    return std::nullopt;
}

// ============================================================================
// Reconnection Logic
// ============================================================================

bool SelfHealing::attempt_reconnect(std::function<bool()> reconnect_func) {
    if (!config_.auto_reconnect) {
        return false;
    }

    // Invoke reconnect start callback
    if (config_.on_reconnect_start) {
        config_.on_reconnect_start();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.reconnection_attempts++;
    }

    bool success = false;
    size_t attempt = 0;

    for (; attempt < config_.max_reconnect_attempts; ++attempt) {
        try {
            // Attempt reconnection
            success = reconnect_func();

            if (success) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.successful_reconnections++;
                    stats_.last_reconnection_time = std::chrono::system_clock::now();
                }

                // Reset circuit breaker on successful reconnection
                if (circuit_breaker_) {
                    circuit_breaker_->reset();
                }

                break;
            }

            // Failed, wait before retry
            if (attempt < config_.max_reconnect_attempts - 1) {
                auto delay = calculate_retry_delay(attempt);
                std::this_thread::sleep_for(delay);
            }

        } catch (const std::exception&) {
            success = false;
            break;
        }
    }

    if (!success) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.failed_reconnections++;
    }

    // Invoke reconnect complete callback
    if (config_.on_reconnect_complete) {
        config_.on_reconnect_complete(success);
    }

    return success;
}

// ============================================================================
// Health Check Logic
// ============================================================================

bool SelfHealing::perform_health_check(std::function<bool()> health_check_func) {
    if (!config_.enable_health_checks) {
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.total_health_checks++;
        stats_.last_health_check_time = std::chrono::system_clock::now();
    }

    bool healthy = false;

    try {
        healthy = health_check_func();

        std::lock_guard<std::mutex> lock(mutex_);
        if (healthy) {
            stats_.consecutive_health_failures = 0;
        } else {
            stats_.failed_health_checks++;
            stats_.consecutive_health_failures++;
        }

    } catch (const std::exception&) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.failed_health_checks++;
        stats_.consecutive_health_failures++;
        healthy = false;
    }

    // Check if we need to trigger reconnection
    if (stats_.consecutive_health_failures >= config_.health_check_failures_before_reconnect) {
        // Trip circuit breaker to prevent further operations
        if (circuit_breaker_) {
            circuit_breaker_->trip();
        }
    }

    return healthy;
}

// ============================================================================
// Statistics and Configuration
// ============================================================================

SelfHealingStats SelfHealing::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void SelfHealing::reset_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = SelfHealingStats{};
    stats_.last_reconnection_time = std::chrono::system_clock::now();
    stats_.last_fallback_time = std::chrono::system_clock::now();
    stats_.last_health_check_time = std::chrono::system_clock::now();
}

SelfHealingConfig SelfHealing::get_config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void SelfHealing::update_config(const SelfHealingConfig& new_config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = new_config;
}

void SelfHealing::set_circuit_breaker(CircuitBreaker* cb) noexcept {
    circuit_breaker_ = cb;
}

CircuitBreaker* SelfHealing::get_circuit_breaker() const noexcept {
    return circuit_breaker_;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void SelfHealing::invoke_fallback(const std::string& operation_name) {
    if (config_.fallback_handler) {
        try {
            config_.fallback_handler(operation_name);
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.fallback_invocations++;
            stats_.last_fallback_time = std::chrono::system_clock::now();
        } catch (const std::exception&) {
            // Fallback handler threw exception, ignore
        }
    }
}

void SelfHealing::update_retry_stats(bool success, std::chrono::milliseconds delay) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.total_retry_time += delay;
    
    if (stats_.total_retry_attempts > 0) {
        stats_.average_retry_delay = std::chrono::milliseconds(
            stats_.total_retry_time.count() / stats_.total_retry_attempts
        );
    }
}

// ============================================================================
// Template Instantiations
// ============================================================================

// Explicitly instantiate common template combinations
template std::error_code SelfHealing::execute_with_retry<std::function<std::error_code()>>(
    std::function<std::error_code()>&& operation,
    const std::string& operation_name
);

template std::optional<uint64_t> SelfHealing::execute_with_retry_result<uint64_t, std::function<std::optional<uint64_t>()>>(
    std::function<std::optional<uint64_t>()>&& operation,
    const std::string& operation_name
);

template std::optional<uint32_t> SelfHealing::execute_with_retry_result<uint32_t, std::function<std::optional<uint32_t>()>>(
    std::function<std::optional<uint32_t>()>&& operation,
    const std::string& operation_name
);

template std::optional<std::vector<uint8_t>> SelfHealing::execute_with_retry_result<std::vector<uint8_t>, std::function<std::optional<std::vector<uint8_t>>()>>(
    std::function<std::optional<std::vector<uint8_t>>()>&& operation,
    const std::string& operation_name
);

} // namespace ArgoSentry
