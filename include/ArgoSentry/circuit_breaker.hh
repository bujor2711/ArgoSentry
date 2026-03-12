// ArgoSentry Circuit Breaker Pattern
// v3.0 - Fault Tolerance & Graceful Degradation
// Prevents cascading failures, allows automatic recovery

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>

namespace ArgoSentry {

//==============================================================================
// Circuit Breaker Errors
//==============================================================================

enum class CircuitBreakerError {
    Success = 0,
    CircuitOpen,           // Circuit is open, operation blocked
    OperationFailed,       // Operation failed in half-open state
    Timeout,               // Operation timed out
    InvalidState          // Invalid state transition
};

// Error category for CircuitBreakerError
class CircuitBreakerErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "circuit_breaker";
    }
    
    std::string message(int ev) const override {
        switch (static_cast<CircuitBreakerError>(ev)) {
            case CircuitBreakerError::Success:
                return "Success";
            case CircuitBreakerError::CircuitOpen:
                return "Circuit breaker is OPEN - operation blocked to prevent cascading failures";
            case CircuitBreakerError::OperationFailed:
                return "Operation failed in HALF_OPEN state";
            case CircuitBreakerError::Timeout:
                return "Operation timed out";
            case CircuitBreakerError::InvalidState:
                return "Invalid circuit breaker state transition";
            default:
                return "Unknown circuit breaker error";
        }
    }
};

inline const CircuitBreakerErrorCategory& circuit_breaker_category() {
    static CircuitBreakerErrorCategory instance;
    return instance;
}

inline std::error_code make_error_code(CircuitBreakerError e) {
    return {static_cast<int>(e), circuit_breaker_category()};
}

// Exception thrown when circuit is open
class CircuitBreakerOpenException : public std::runtime_error {
public:
    CircuitBreakerOpenException()
        : std::runtime_error("Circuit breaker is OPEN - operation blocked") {}
};

//==============================================================================
// Circuit Breaker States
//==============================================================================

enum class CircuitState {
    CLOSED,      // Normal operation - requests pass through
    OPEN,        // Failure detected - requests blocked immediately
    HALF_OPEN    // Testing recovery - limited requests allowed
};

inline const char* to_string(CircuitState state) {
    switch (state) {
        case CircuitState::CLOSED:    return "CLOSED";
        case CircuitState::OPEN:      return "OPEN";
        case CircuitState::HALF_OPEN: return "HALF_OPEN";
        default:                      return "UNKNOWN";
    }
}

//==============================================================================
// Circuit Breaker Configuration
//==============================================================================

struct CircuitBreakerConfig {
    // Failure threshold before opening circuit
    size_t failure_threshold = 5;
    
    // How long circuit stays OPEN before transitioning to HALF_OPEN
    std::chrono::seconds open_timeout{30};
    
    // Number of successful calls required in HALF_OPEN to close circuit
    size_t success_threshold = 2;
    
    // Time window for counting failures (rolling window)
    std::chrono::seconds failure_window{60};
    
    // Maximum time to wait for operation completion
    std::chrono::milliseconds operation_timeout{5000};
    
    // Enable automatic recovery attempts
    bool auto_recovery = true;
    
    // Callback when circuit state changes
    using StateChangeCallback = std::function<void(CircuitState old_state, CircuitState new_state)>;
    StateChangeCallback on_state_change;
};

//==============================================================================
// Circuit Breaker Statistics
//==============================================================================

struct CircuitBreakerStats {
    size_t total_calls = 0;
    size_t successful_calls = 0;
    size_t failed_calls = 0;
    size_t rejected_calls = 0;      // Calls rejected when circuit is OPEN
    size_t state_transitions = 0;    // Number of state changes
    
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_state_change;
    
    // Current state info
    CircuitState current_state = CircuitState::CLOSED;
    size_t consecutive_failures = 0;
    size_t consecutive_successes = 0;
    
    // Calculate success rate (0.0 - 1.0)
    [[nodiscard]] double get_success_rate() const {
        if (total_calls == 0) return 1.0;
        return static_cast<double>(successful_calls) / total_calls;
    }
    
    // Calculate failure rate (0.0 - 1.0)
    [[nodiscard]] double get_failure_rate() const {
        return 1.0 - get_success_rate();
    }
};

//==============================================================================
// Circuit Breaker
//==============================================================================

class CircuitBreaker {
public:
    // Constructor with configuration
    explicit CircuitBreaker(CircuitBreakerConfig config = {});
    
    // Destructor
    ~CircuitBreaker() = default;
    
    // Prevent copying
    CircuitBreaker(const CircuitBreaker&) = delete;
    CircuitBreaker& operator=(const CircuitBreaker&) = delete;
    
    // Allow moving
    CircuitBreaker(CircuitBreaker&&) noexcept = default;
    CircuitBreaker& operator=(CircuitBreaker&&) noexcept = default;
    
    //==========================================================================
    // Execute Operations with Circuit Breaker Protection
    //==========================================================================
    
    // Execute operation with circuit breaker protection (returns error code)
    template<typename Func>
    [[nodiscard]] std::error_code execute(Func&& operation) noexcept {
        // Check if circuit allows request
        if (!allow_request()) {
            record_rejected();
            return make_error_code(CircuitBreakerError::CircuitOpen);
        }
        
        // Execute operation
        try {
            operation();
            record_success();
            return make_error_code(CircuitBreakerError::Success);
        }
        catch (...) {
            record_failure();
            return make_error_code(CircuitBreakerError::OperationFailed);
        }
    }
    
    // Execute operation with return value (throws on circuit open)
    template<typename Func>
    auto execute_with_result(Func&& operation) -> decltype(operation()) {
        // Check if circuit allows request
        if (!allow_request()) {
            record_rejected();
            throw CircuitBreakerOpenException();
        }
        
        // Execute operation
        try {
            auto result = operation();
            record_success();
            return result;
        }
        catch (...) {
            record_failure();
            throw;  // Re-throw original exception
        }
    }
    
    //==========================================================================
    // State Management
    //==========================================================================
    
    // Get current circuit state
    [[nodiscard]] CircuitState get_state() const noexcept {
        return state_.load(std::memory_order_acquire);
    }
    
    // Check if circuit is open
    [[nodiscard]] bool is_open() const noexcept {
        return get_state() == CircuitState::OPEN;
    }
    
    // Check if circuit is closed
    [[nodiscard]] bool is_closed() const noexcept {
        return get_state() == CircuitState::CLOSED;
    }
    
    // Check if circuit is half-open
    [[nodiscard]] bool is_half_open() const noexcept {
        return get_state() == CircuitState::HALF_OPEN;
    }
    
    //==========================================================================
    // Manual Control
    //==========================================================================
    
    // Manually trip circuit (force OPEN)
    void trip() noexcept;
    
    // Manually reset circuit (force CLOSED)
    void reset() noexcept;
    
    // Manually set half-open state (test recovery)
    void half_open() noexcept;
    
    //==========================================================================
    // Statistics
    //==========================================================================
    
    // Get current statistics
    [[nodiscard]] CircuitBreakerStats get_stats() const noexcept;
    
    // Get failure count
    [[nodiscard]] size_t get_failure_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_.consecutive_failures;
    }
    
    // Get last failure time
    [[nodiscard]] std::chrono::system_clock::time_point get_last_failure() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_.last_failure_time;
    }
    
    // Reset statistics
    void reset_stats() noexcept;
    
    //==========================================================================
    // Configuration
    //==========================================================================
    
    // Get configuration
    [[nodiscard]] const CircuitBreakerConfig& get_config() const noexcept {
        return config_;
    }
    
    // Update configuration (thread-safe)
    void update_config(CircuitBreakerConfig new_config) noexcept;
    
private:
    //==========================================================================
    // Internal Methods
    //==========================================================================
    
    // Check if request should be allowed
    [[nodiscard]] bool allow_request() noexcept;
    
    // Record successful operation
    void record_success() noexcept;
    
    // Record failed operation
    void record_failure() noexcept;
    
    // Record rejected operation (circuit open)
    void record_rejected() noexcept;
    
    // Transition to new state
    void transition_state(CircuitState new_state) noexcept;
    
    // Check if should transition to HALF_OPEN
    [[nodiscard]] bool should_attempt_reset() const noexcept;
    
    //==========================================================================
    // Member Variables
    //==========================================================================
    
    CircuitBreakerConfig config_;
    std::atomic<CircuitState> state_{CircuitState::CLOSED};
    
    mutable std::mutex mutex_;  // Protects stats_
    CircuitBreakerStats stats_;
};

} // namespace ArgoSentry

// Register error code with std::error_code
namespace std {
    template<>
    struct is_error_code_enum<ArgoSentry::CircuitBreakerError> : true_type {};
}
