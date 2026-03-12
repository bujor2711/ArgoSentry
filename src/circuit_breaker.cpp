// circuit_breaker.cpp - Circuit Breaker Pattern Implementation
// Part of ArgoSentry v3.0 - Health Monitoring & Fault Tolerance
// Prevents cascading failures and enables graceful degradation

#include <ArgoSentry/circuit_breaker.hh>

namespace ArgoSentry {

//==============================================================================
// CircuitBreaker Implementation (Non-Inline Methods Only)
//==============================================================================

CircuitBreaker::CircuitBreaker(CircuitBreakerConfig config)
    : config_(std::move(config))
    , state_(CircuitState::CLOSED)
    , mutex_()
    , stats_()
{
    // Initialize stats with current state
    stats_.current_state = CircuitState::CLOSED;
    stats_.last_state_change = std::chrono::system_clock::now();
}

//------------------------------------------------------------------------------
// Core Request Handling
//------------------------------------------------------------------------------

bool CircuitBreaker::allow_request() noexcept {
    auto current_state = state_.load(std::memory_order_acquire);

    // CLOSED: Always allow
    if (current_state == CircuitState::CLOSED) {
        return true;
    }

    // OPEN: Check if timeout elapsed for transition to HALF_OPEN
    if (current_state == CircuitState::OPEN) {
        if (should_attempt_reset()) {
            transition_state(CircuitState::HALF_OPEN);
            return true;  // Allow request in HALF_OPEN to test recovery
        }
        return false;  // Still OPEN, reject request
    }

    // HALF_OPEN: Allow request to test recovery
    return true;
}

//------------------------------------------------------------------------------
// Record Outcomes
//------------------------------------------------------------------------------

void CircuitBreaker::record_success() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Update counters
    stats_.total_calls++;
    stats_.successful_calls++;
    stats_.consecutive_successes++;
    stats_.consecutive_failures = 0;  // Reset failure count
    stats_.last_success_time = std::chrono::system_clock::now();

    // Check HALF_OPEN → CLOSED transition
    auto current_state = state_.load(std::memory_order_acquire);
    if (current_state == CircuitState::HALF_OPEN) {
        if (stats_.consecutive_successes >= config_.success_threshold) {
            // Enough successes to close circuit
            transition_state(CircuitState::CLOSED);
        }
    }
}

void CircuitBreaker::record_failure() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Update counters
    stats_.total_calls++;
    stats_.failed_calls++;
    stats_.consecutive_failures++;
    stats_.consecutive_successes = 0;  // Reset success count
    stats_.last_failure_time = std::chrono::system_clock::now();

    // Check for state transitions
    auto current_state = state_.load(std::memory_order_acquire);

    // CLOSED → OPEN: Too many failures
    if (current_state == CircuitState::CLOSED) {
        if (stats_.consecutive_failures >= config_.failure_threshold) {
            transition_state(CircuitState::OPEN);
        }
    }
    // HALF_OPEN → OPEN: Test failed
    else if (current_state == CircuitState::HALF_OPEN) {
        if (stats_.consecutive_failures >= config_.failure_threshold) {
            transition_state(CircuitState::OPEN);
        }
    }
}

void CircuitBreaker::record_rejected() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.rejected_calls++;
}

//------------------------------------------------------------------------------
// State Transitions
//------------------------------------------------------------------------------

void CircuitBreaker::transition_state(CircuitState new_state) noexcept {
    auto old_state = state_.exchange(new_state, std::memory_order_acq_rel);

    if (old_state != new_state) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Update statistics
        stats_.current_state = new_state;
        stats_.state_transitions++;
        stats_.last_state_change = std::chrono::system_clock::now();

        // Reset consecutive counters on state change
        if (new_state == CircuitState::CLOSED) {
            stats_.consecutive_failures = 0;
        }
        if (new_state == CircuitState::HALF_OPEN) {
            stats_.consecutive_successes = 0;
        }

        // Invoke state change callback if configured
        if (config_.on_state_change) {
            try {
                config_.on_state_change(old_state, new_state);
            } catch (...) {
                // Ignore exceptions from callbacks
                // Don't let user code break our state machine
            }
        }
    }
}

bool CircuitBreaker::should_attempt_reset() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - stats_.last_state_change
    );

    return elapsed >= config_.open_timeout;
}

//------------------------------------------------------------------------------
// Manual Control
//------------------------------------------------------------------------------

void CircuitBreaker::trip() noexcept {
    transition_state(CircuitState::OPEN);
}

void CircuitBreaker::reset() noexcept {
    transition_state(CircuitState::CLOSED);
    reset_stats();
}

void CircuitBreaker::half_open() noexcept {
    transition_state(CircuitState::HALF_OPEN);
}

//------------------------------------------------------------------------------
// Statistics
//------------------------------------------------------------------------------

CircuitBreakerStats CircuitBreaker::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;  // Return copy (thread-safe)
}

void CircuitBreaker::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto current_state = state_.load(std::memory_order_acquire);

    // Reset all counters
    stats_ = CircuitBreakerStats{};

    // Restore current state info
    stats_.current_state = current_state;
    stats_.last_state_change = std::chrono::system_clock::now();
}

//------------------------------------------------------------------------------
// Configuration
//------------------------------------------------------------------------------

void CircuitBreaker::update_config(CircuitBreakerConfig new_config) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = std::move(new_config);
}

} // namespace ArgoSentry
