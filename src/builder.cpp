#include "ArgoSentry/builder.hh"
#include "ArgoSentry/dma.hh"
#include "ArgoSentry/logger.hh"
#include "ArgoSentry/log_sinks.hh"
#include <stdexcept>
#include <sstream>

namespace ArgoSentry {

// Constructor with sensible defaults
DMABuilder::DMABuilder()
    : use_memory_map_(true)
    , fpga_algorithm_(0)
    , cache_size_(100 * 1024 * 1024)  // 100MB default
    , cache_ttl_(30)  // 30 seconds
    , enable_metrics_(false)
    , enable_health_monitoring_(false)
    , auto_start_health_monitoring_(false)
    , logging_level_(2)  // Warning level (legacy)
    , scan_chunk_size_(1 * 1024 * 1024)  // 1MB chunks
    , max_read_size_(10 * 1024 * 1024)  // 10MB max
    , rate_limit_bytes_per_sec_(0)  // v2.3: Unlimited by default
    , logger_(nullptr)  // v2.9: No logger by default
    , file_logging_enabled_(false)
    , console_logging_enabled_(false)
    , log_level_(LogLevel::INFO)
    , console_log_level_(LogLevel::WARN)
    , log_filepath_("argosentry.log")
{
}

DMABuilder& DMABuilder::with_memory_map(bool enable) {
    use_memory_map_ = enable;
    return *this;
}

DMABuilder& DMABuilder::with_fpga_algorithm(int algo) {
    if (algo < 0 || algo > 1) {
        throw std::invalid_argument("FPGA algorithm must be 0 (default) or 1 (alternative)");
    }
    fpga_algorithm_ = algo;
    return *this;
}

DMABuilder& DMABuilder::with_cache(size_t size, std::chrono::seconds ttl) {
    if (ttl.count() < 1) {
        throw std::invalid_argument("Cache TTL must be at least 1 second");
    }
    cache_size_ = size;
    cache_ttl_ = ttl;
    return *this;
}

DMABuilder& DMABuilder::with_metrics(bool enable) {
    enable_metrics_ = enable;
    return *this;
}

DMABuilder& DMABuilder::with_health_monitoring(bool enable, bool auto_start) {
    enable_health_monitoring_ = enable;
    auto_start_health_monitoring_ = auto_start;
    return *this;
}

DMABuilder& DMABuilder::with_logging(int level) {
    if (level < 0 || level > 4) {
        throw std::invalid_argument("Logging level must be 0-4 (0=none, 1=error, 2=warning, 3=info, 4=debug)");
    }
    logging_level_ = level;
    return *this;
}

DMABuilder& DMABuilder::with_logging(LogLevel level, const std::string& filepath) {
    file_logging_enabled_ = true;
    log_level_ = level;
    log_filepath_ = filepath;

    // Create logger if not exists
    if (!logger_) {
        logger_ = Logger::create();
    }

    // Add file sink with size-based rotation (10MB, keep 5 files)
    logger_->add_sink(std::make_unique<FileSink>(
        filepath,
        level,
        FileSink::RotationPolicy::SIZE,
        10 * 1024 * 1024,  // 10MB rotation
        5                   // Keep 5 files
    ));

    return *this;
}

DMABuilder& DMABuilder::with_console_logging(LogLevel level) {
    console_logging_enabled_ = true;
    console_log_level_ = level;

    // Create logger if not exists
    if (!logger_) {
        logger_ = Logger::create();
    }

    // Add console sink with colors enabled
    logger_->add_sink(std::make_unique<ConsoleSink>(
        level,
        true  // Colors enabled
    ));

    return *this;
}

DMABuilder& DMABuilder::with_scan_chunk_size(size_t chunk_size) {
    if (chunk_size < 4096) {
        throw std::invalid_argument("Scan chunk size must be at least 4KB");
    }
    if (chunk_size > 100 * 1024 * 1024) {
        throw std::invalid_argument("Scan chunk size cannot exceed 100MB");
    }
    scan_chunk_size_ = chunk_size;
    return *this;
}

DMABuilder& DMABuilder::with_max_read_size(size_t max_size) {
    if (max_size < 1024) {
        throw std::invalid_argument("Max read size must be at least 1KB");
    }
    if (max_size > 100 * 1024 * 1024) {
        throw std::invalid_argument("Max read size cannot exceed 100MB");
    }
    max_read_size_ = max_size;
    return *this;
}

DMABuilder& DMABuilder::with_rate_limit(size_t bytes_per_sec) {
    rate_limit_bytes_per_sec_ = bytes_per_sec;
    return *this;
}

DMABuilder& DMABuilder::with_circuit_breaker(
    size_t failure_threshold,
    unsigned int timeout_seconds
) {
    if (failure_threshold == 0) {
        throw std::invalid_argument("Circuit breaker failure threshold must be at least 1");
    }
    if (timeout_seconds == 0) {
        throw std::invalid_argument("Circuit breaker timeout must be at least 1 second");
    }

    circuit_breaker_enabled_ = true;
    circuit_breaker_failure_threshold_ = failure_threshold;
    circuit_breaker_timeout_seconds_ = timeout_seconds;
    return *this;
}

DMABuilder& DMABuilder::with_self_healing(
    size_t max_retries,
    unsigned int initial_delay_ms,
    int policy
) {
    if (max_retries == 0) {
        throw std::invalid_argument("Self-healing max retries must be at least 1");
    }
    if (initial_delay_ms == 0) {
        throw std::invalid_argument("Self-healing initial delay must be at least 1 millisecond");
    }
    if (policy < 0 || policy > 4) {  // RetryPolicy enum range: NONE(0) to FIBONACCI(4)
        throw std::invalid_argument("Invalid retry policy (must be 0-4)");
    }

    self_healing_enabled_ = true;
    self_healing_max_retries_ = max_retries;
    self_healing_initial_delay_ms_ = initial_delay_ms;
    self_healing_policy_ = policy;
    return *this;
}

DMABuilder& DMABuilder::with_pointer_resolver(bool enable) {
    pointer_resolver_enabled_ = enable;
    return *this;
}

std::unique_ptr<DMA> DMABuilder::build() const {
    // Validate configuration before building
    if (!is_valid()) {
        throw std::runtime_error("Invalid DMA configuration: " + get_validation_error());
    }

    // Create DMA instance with memory map setting and logger
    auto dma = std::make_unique<DMA>(use_memory_map_, logger_);

    // Configure cache
    if (cache_size_ > 0) {
        dma->enable_cache(true);
        dma->set_cache_size(cache_size_);
        dma->set_cache_ttl(cache_ttl_);
    } else {
        dma->enable_cache(false);
    }

    // Configure metrics
    if (enable_metrics_) {
        dma->enable_metrics(true);
    }

    // Configure health monitoring
    if (enable_health_monitoring_) {
        dma->enable_health_monitoring(true);
        if (auto_start_health_monitoring_) {
            dma->start_automatic_health_monitoring();
        }
    }

    // Configure rate limiting (v2.3)
    if (rate_limit_bytes_per_sec_ > 0) {
        dma->enable_rate_limiting(true);
        dma->set_rate_limit(rate_limit_bytes_per_sec_);
    }

    // TODO: Apply other configurations when DMA class exposes setters
    // - fpga_algorithm_
    // - logging_level_
    // - scan_chunk_size_
    // - max_read_size_

    return dma;
}

DMABuilder DMABuilder::production() {
    return DMABuilder()
        .with_memory_map(true)
        .with_fpga_algorithm(0)
        .with_cache(100 * 1024 * 1024, std::chrono::seconds(60))  // 100MB, 1 min TTL
        .with_metrics(true)
        .with_health_monitoring(true, true)  // Enable with auto-start
        .with_logging(2)  // Warning level
        .with_scan_chunk_size(1 * 1024 * 1024)  // 1MB chunks
        .with_max_read_size(10 * 1024 * 1024);  // 10MB max
}

DMABuilder DMABuilder::development() {
    return DMABuilder()
        .with_memory_map(true)
        .with_fpga_algorithm(0)
        .with_cache(10 * 1024 * 1024, std::chrono::seconds(5))  // 10MB, 5s TTL
        .with_metrics(true)
        .with_health_monitoring(false)  // No background monitoring in dev
        .with_logging(4)  // Debug level
        .with_scan_chunk_size(512 * 1024)  // 512KB chunks
        .with_max_read_size(5 * 1024 * 1024);  // 5MB max
}

DMABuilder DMABuilder::testing() {
    return DMABuilder()
        .with_memory_map(false)  // Mock mode
        .with_fpga_algorithm(0)
        .with_cache(0, std::chrono::seconds(1))  // Disable cache
        .with_metrics(false)  // No overhead
        .with_health_monitoring(false)
        .with_logging(0)  // No logging
        .with_scan_chunk_size(64 * 1024)  // Small chunks
        .with_max_read_size(1 * 1024 * 1024);  // 1MB max
}

bool DMABuilder::is_valid() const {
    return validate_cache_config() &&
           validate_fpga_config() &&
           validate_scan_config();
}

std::string DMABuilder::get_validation_error() const {
    if (!validate_cache_config()) {
        return "Invalid cache configuration";
    }
    if (!validate_fpga_config()) {
        return "Invalid FPGA configuration";
    }
    if (!validate_scan_config()) {
        return "Invalid scan configuration";
    }
    return "";
}

bool DMABuilder::validate_cache_config() const {
    // Cache size can be 0 (disabled) or reasonable value
    if (cache_size_ > 0 && cache_size_ < 1024) {
        return false;  // Too small to be useful
    }
    if (cache_size_ > 1024ULL * 1024 * 1024) {  // 1GB
        return false;  // Too large
    }

    // TTL must be reasonable
    if (cache_ttl_.count() < 1 || cache_ttl_.count() > 3600) {
        return false;  // Must be 1s - 1 hour
    }

    return true;
}

bool DMABuilder::validate_fpga_config() const {
    // FPGA algorithm must be valid
    if (fpga_algorithm_ < 0 || fpga_algorithm_ > 1) {
        return false;
    }
    return true;
}

bool DMABuilder::validate_scan_config() const {
    // Scan chunk size must be reasonable
    if (scan_chunk_size_ < 4096 || scan_chunk_size_ > 100 * 1024 * 1024) {
        return false;
    }

    // Max read size must be reasonable
    if (max_read_size_ < 1024 || max_read_size_ > 100 * 1024 * 1024) {
        return false;
    }

    // Scan chunk size should not exceed max read size
    if (scan_chunk_size_ > max_read_size_) {
        return false;
    }

    return true;
}

} // namespace ArgoSentry


