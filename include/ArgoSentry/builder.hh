#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace ArgoSentry {

// Forward declarations
class DMA;
class Logger;
enum class LogLevel;

/**
 * @brief Fluent builder interface for DMA configuration
 * 
 * Provides an elegant, type-safe way to configure DMA objects using method chaining.
 * Eliminates the need for multiple setter calls and makes configuration intent clear.
 * 
 * Example usage:
 * @code
 * auto dma = DMA::Builder()
 *     .with_cache(100 * 1024 * 1024, std::chrono::seconds(60))
 *     .with_metrics(true)
 *     .with_health_monitoring(true)
 *     .build();
 * @endcode
 * 
 * @since v2.2
 */
class DMABuilder {
public:
    /**
     * @brief Default constructor with sensible defaults
     */
    DMABuilder();

    /**
     * @brief Enable/disable memory map usage
     * @param enable True to use memory map (default), false for direct access
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_memory_map(bool enable);

    /**
     * @brief Set FPGA algorithm
     * @param algo Algorithm ID (0 = default, 1 = alternative)
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_fpga_algorithm(int algo);

    /**
     * @brief Configure memory cache
     * @param size Cache size in bytes (0 to disable, default: 100MB)
     * @param ttl Time-to-live for cached entries (default: 30s)
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_cache(size_t size, std::chrono::seconds ttl = std::chrono::seconds(30));

    /**
     * @brief Enable/disable performance metrics collection
     * @param enable True to enable metrics (default: false)
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_metrics(bool enable);

    /**
     * @brief Enable/disable health monitoring
     * @param enable True to enable health monitoring (default: false)
     * @param auto_start True to start automatic background monitoring (default: false)
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_health_monitoring(bool enable, bool auto_start = false);

    /**
     * @brief Configure logging level (legacy int-based)
     * @param level Logging level (0=none, 1=error, 2=warning, 3=info, 4=debug)
     * @return Reference to this builder for chaining
     * @deprecated Use with_logging(LogLevel, filepath) instead
     */
    DMABuilder& with_logging(int level);

    /**
     * @brief Configure file logging with level and path
     * @param level Minimum log level (DEBUG, INFO, WARN, ERR, FATAL)
     * @param filepath Path to log file (default: "argosentry.log")
     * @return Reference to this builder for chaining
     * @since v2.9
     * 
     * Example:
     * @code
     * auto dma = DMABuilder()
     *     .with_logging(LogLevel::INFO, "dma.log")
     *     .build();
     * @endcode
     */
    DMABuilder& with_logging(LogLevel level, const std::string& filepath = "argosentry.log");

    /**
     * @brief Configure console logging with colors
     * @param level Minimum log level for console output
     * @return Reference to this builder for chaining
     * @since v2.9
     * 
     * Example:
     * @code
     * auto dma = DMABuilder()
     *     .with_console_logging(LogLevel::WARN)
     *     .build();
     * @endcode
     */
    DMABuilder& with_console_logging(LogLevel level);

    /**
     * @brief Set chunk size for signature scanning
     * @param chunk_size Chunk size in bytes (default: 1MB)
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_scan_chunk_size(size_t chunk_size);

    /**
     * @brief Set maximum read size for single operations
     * @param max_size Maximum size in bytes (default: 10MB)
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_max_read_size(size_t max_size);

    /**
     * @brief Configure rate limiting for DMA operations
     * @param bytes_per_sec Maximum bytes per second (0 = unlimited)
     * @return Reference to this builder for chaining
     * @since v2.3
     * 
     * Example:
     * @code
     * auto dma = DMA::Builder()
     *     .with_rate_limit(1 * 1024 * 1024)  // 1 MB/s limit
     *     .build();
     * @endcode
     * 
     * Thread-safe: Rate limiting works correctly in multi-threaded scenarios.
     * Overhead: ~2-5% when enabled, 0% when disabled (bytes_per_sec = 0).
     */
    DMABuilder& with_rate_limit(size_t bytes_per_sec);

    /**
     * @brief Configure circuit breaker for fault tolerance
     * @param failure_threshold Number of failures before opening circuit (default: 5)
     * @param timeout_seconds Seconds in OPEN state before attempting recovery (default: 30)
     * @return Reference to this builder for chaining
     * @since v3.0
     * 
     * Example:
     * @code
     * auto dma = DMABuilder()
     *     .with_circuit_breaker(10, 60)  // 10 failures, 60s timeout
     *     .build();
     * @endcode
     * 
     * Circuit breaker prevents cascading failures by temporarily blocking
     * operations when failure threshold is reached. Automatically attempts
     * recovery after timeout period.
     * 
     * States:
     * - CLOSED: Normal operation, operations allowed
     * - OPEN: Blocking mode, operations rejected (after threshold)
     * - HALF_OPEN: Testing recovery, limited operations allowed
     */
    DMABuilder& with_circuit_breaker(
        size_t failure_threshold = 5,
        unsigned int timeout_seconds = 30
    );

    /**
     * @brief Build DMA object with configured settings
     * @return Fully configured DMA instance (unique_ptr)
     * @throws std::runtime_error if configuration is invalid
     */
    std::unique_ptr<DMA> build() const;

    /**
     * @brief Create builder with production-ready defaults
     * @return Builder configured for production use
     */
    static DMABuilder production();

    /**
     * @brief Create builder with development/debug defaults
     * @return Builder configured for development use
     */
    static DMABuilder development();

    /**
     * @brief Create builder with testing defaults (minimal overhead)
     * @return Builder configured for unit testing
     */
    static DMABuilder testing();

    /**
     * @brief Validate current configuration
     * @return True if configuration is valid
     */
    bool is_valid() const;

    /**
     * @brief Get validation error message
     * @return Error message if invalid, empty string if valid
     */
    std::string get_validation_error() const;

private:
    // Configuration state
    bool use_memory_map_;
    int fpga_algorithm_;
    size_t cache_size_;
    std::chrono::seconds cache_ttl_;
    bool enable_metrics_;
    bool enable_health_monitoring_;
    bool auto_start_health_monitoring_;
    int logging_level_;  // Legacy int-based logging
    size_t scan_chunk_size_;
    size_t max_read_size_;
    size_t rate_limit_bytes_per_sec_;  // v2.3: Rate limiting

    // v2.9: Logging framework members
    std::shared_ptr<Logger> logger_;        // Logger instance
    bool file_logging_enabled_{false};      // File logging flag
    bool console_logging_enabled_{false};   // Console logging flag
    LogLevel log_level_;                    // File log level
    LogLevel console_log_level_;            // Console log level
    std::string log_filepath_;              // Log file path

    // v3.0: Circuit breaker members
    bool circuit_breaker_enabled_{true};                // Circuit breaker enabled by default
    size_t circuit_breaker_failure_threshold_{5};       // Failures before opening
    unsigned int circuit_breaker_timeout_seconds_{30};  // Timeout in OPEN state

    // Validation helpers
    bool validate_cache_config() const;
    bool validate_fpga_config() const;
    bool validate_scan_config() const;
};

} // namespace ArgoSentry
