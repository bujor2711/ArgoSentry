#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <mutex>

namespace VolkDMA {
namespace Metrics {

// Thread-safe metrics structure
struct DMAMetrics {
    // Read statistics
    std::atomic<size_t> total_bytes_read{0};
    std::atomic<size_t> total_read_operations{0};
    std::atomic<size_t> failed_read_operations{0};
    std::atomic<size_t> partial_read_operations{0};
    
    // Timing (microseconds)
    std::atomic<uint64_t> total_read_time_us{0};
    std::atomic<uint64_t> min_read_time_us{UINT64_MAX};
    std::atomic<uint64_t> max_read_time_us{0};
    
    // Signature scanning statistics
    std::atomic<size_t> total_signatures_scanned{0};
    std::atomic<size_t> signatures_found{0};
    std::atomic<size_t> signatures_not_found{0};
    std::atomic<uint64_t> total_scan_time_us{0};
    std::atomic<uint64_t> total_scan_bytes{0};
    
    // Cache statistics (prepared for future cache implementation)
    std::atomic<size_t> cache_hits{0};
    std::atomic<size_t> cache_misses{0};
    
    // Process operations
    std::atomic<size_t> process_lookups{0};
    std::atomic<size_t> successful_process_lookups{0};
    
    // Computed metrics (not atomic, use getter methods)
    double get_throughput_mbps() const;
    double get_average_read_time_us() const;
    double get_success_rate() const;
    double get_cache_hit_ratio() const;
    double get_scan_throughput_mbps() const;
    double get_signature_success_rate() const;
    
    // Statistics
    size_t get_successful_reads() const;
    size_t get_total_operations() const;
    
    // Reset all metrics
    void reset();
    
    // String representation
    std::string to_string() const;
    std::string to_summary_string() const;
};

// Metrics collector for recording operations
class MetricsCollector {
public:
    MetricsCollector();
    
    // Record read operation
    void record_read(size_t bytes, uint64_t duration_us, bool success, bool partial = false);
    
    // Record signature scan operation
    void record_scan(size_t bytes_scanned, uint64_t duration_us, bool found);
    
    // Record cache operation
    void record_cache_hit();
    void record_cache_miss();
    
    // Record process lookup
    void record_process_lookup(bool success);
    
    // Get current metrics (thread-safe)
    const DMAMetrics& get_metrics() const;
    
    // Reset all metrics
    void reset_metrics();
    
    // Enable/disable metrics collection
    void enable();
    void disable();
    bool is_enabled() const;
    
    // Logging
    void log_summary() const;
    void log_detailed() const;
    
private:
    DMAMetrics metrics_;
    std::atomic<bool> enabled_{true};
    mutable std::mutex log_mutex_;
    
    // Helper to update min/max atomically
    void update_min_time(uint64_t new_time);
    void update_max_time(uint64_t new_time);
};

// RAII timer for automatic metric recording
class ScopedTimer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    
    ScopedTimer();
    
    // Get elapsed time in microseconds
    uint64_t elapsed_us() const;
    
    // Reset timer
    void reset();
    
private:
    TimePoint start_time_;
};

// Helper functions
namespace Utils {
    // Convert bytes to megabytes
    inline double bytes_to_mb(size_t bytes) {
        return static_cast<double>(bytes) / (1024.0 * 1024.0);
    }
    
    // Convert microseconds to seconds
    inline double us_to_seconds(uint64_t microseconds) {
        return static_cast<double>(microseconds) / 1000000.0;
    }
    
    // Calculate throughput in MB/s
    inline double calculate_throughput_mbps(size_t bytes, uint64_t microseconds) {
        if (microseconds == 0) return 0.0;
        double seconds = us_to_seconds(microseconds);
        return bytes_to_mb(bytes) / seconds;
    }
    
    // Format time duration
    std::string format_duration(uint64_t microseconds);
    
    // Format bytes
    std::string format_bytes(size_t bytes);
}

} // namespace Metrics
} // namespace VolkDMA
