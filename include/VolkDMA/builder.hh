#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace VolkDMA {

// Forward declaration
class DMA;

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
     * @brief Configure logging level
     * @param level Logging level (0=none, 1=error, 2=warning, 3=info, 4=debug)
     * @return Reference to this builder for chaining
     */
    DMABuilder& with_logging(int level);

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
    int logging_level_;
    size_t scan_chunk_size_;
    size_t max_read_size_;

    // Validation helpers
    bool validate_cache_config() const;
    bool validate_fpga_config() const;
    bool validate_scan_config() const;
};

} // namespace VolkDMA
