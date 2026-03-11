#include "include/VolkDMA/metrics.hh"

#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace VolkDMA {
namespace Metrics {

// ============================================================================
// DMAMetrics Implementation
// ============================================================================

double DMAMetrics::get_throughput_mbps() const {
    uint64_t time = total_read_time_us.load();
    if (time == 0) return 0.0;
    
    size_t bytes = total_bytes_read.load();
    return Utils::calculate_throughput_mbps(bytes, time);
}

double DMAMetrics::get_average_read_time_us() const {
    size_t ops = total_read_operations.load();
    if (ops == 0) return 0.0;
    
    uint64_t total_time = total_read_time_us.load();
    return static_cast<double>(total_time) / static_cast<double>(ops);
}

double DMAMetrics::get_success_rate() const {
    size_t total = total_read_operations.load();
    if (total == 0) return 0.0;
    
    size_t failed = failed_read_operations.load();
    size_t successful = total - failed;
    return (static_cast<double>(successful) / static_cast<double>(total)) * 100.0;
}

double DMAMetrics::get_cache_hit_ratio() const {
    size_t hits = cache_hits.load();
    size_t misses = cache_misses.load();
    size_t total = hits + misses;
    
    if (total == 0) return 0.0;
    return (static_cast<double>(hits) / static_cast<double>(total)) * 100.0;
}

double DMAMetrics::get_scan_throughput_mbps() const {
    uint64_t time = total_scan_time_us.load();
    if (time == 0) return 0.0;
    
    size_t bytes = total_scan_bytes.load();
    return Utils::calculate_throughput_mbps(bytes, time);
}

double DMAMetrics::get_signature_success_rate() const {
    size_t total = total_signatures_scanned.load();
    if (total == 0) return 0.0;
    
    size_t found = signatures_found.load();
    return (static_cast<double>(found) / static_cast<double>(total)) * 100.0;
}

size_t DMAMetrics::get_successful_reads() const {
    size_t total = total_read_operations.load();
    size_t failed = failed_read_operations.load();
    return total - failed;
}

size_t DMAMetrics::get_total_operations() const {
    return total_read_operations.load() + total_signatures_scanned.load();
}

void DMAMetrics::reset() {
    total_bytes_read = 0;
    total_read_operations = 0;
    failed_read_operations = 0;
    partial_read_operations = 0;
    
    total_read_time_us = 0;
    min_read_time_us = UINT64_MAX;
    max_read_time_us = 0;
    
    total_signatures_scanned = 0;
    signatures_found = 0;
    signatures_not_found = 0;
    total_scan_time_us = 0;
    total_scan_bytes = 0;
    
    cache_hits = 0;
    cache_misses = 0;
    
    process_lookups = 0;
    successful_process_lookups = 0;
}

std::string DMAMetrics::to_string() const {
    std::ostringstream oss;
    
    oss << "=== DMA Performance Metrics ===" << std::endl;
    
    // Read statistics
    oss << "\n[Read Operations]" << std::endl;
    oss << "  Total operations:    " << total_read_operations.load() << std::endl;
    oss << "  Successful:          " << get_successful_reads() << std::endl;
    oss << "  Failed:              " << failed_read_operations.load() << std::endl;
    oss << "  Partial:             " << partial_read_operations.load() << std::endl;
    oss << "  Success rate:        " << std::fixed << std::setprecision(2) 
        << get_success_rate() << "%" << std::endl;
    oss << "  Total bytes read:    " << Utils::format_bytes(total_bytes_read.load()) << std::endl;
    oss << "  Throughput:          " << std::fixed << std::setprecision(2) 
        << get_throughput_mbps() << " MB/s" << std::endl;
    
    // Timing
    oss << "\n[Read Timing]" << std::endl;
    oss << "  Average time:        " << Utils::format_duration(static_cast<uint64_t>(get_average_read_time_us())) << std::endl;
    
    uint64_t min_time = min_read_time_us.load();
    uint64_t max_time = max_read_time_us.load();
    
    if (min_time != UINT64_MAX) {
        oss << "  Min time:            " << Utils::format_duration(min_time) << std::endl;
    }
    if (max_time > 0) {
        oss << "  Max time:            " << Utils::format_duration(max_time) << std::endl;
    }
    oss << "  Total time:          " << Utils::format_duration(total_read_time_us.load()) << std::endl;
    
    // Signature scanning
    oss << "\n[Signature Scanning]" << std::endl;
    oss << "  Total scans:         " << total_signatures_scanned.load() << std::endl;
    oss << "  Found:               " << signatures_found.load() << std::endl;
    oss << "  Not found:           " << signatures_not_found.load() << std::endl;
    oss << "  Success rate:        " << std::fixed << std::setprecision(2) 
        << get_signature_success_rate() << "%" << std::endl;
    oss << "  Bytes scanned:       " << Utils::format_bytes(total_scan_bytes.load()) << std::endl;
    oss << "  Scan throughput:     " << std::fixed << std::setprecision(2) 
        << get_scan_throughput_mbps() << " MB/s" << std::endl;
    oss << "  Total scan time:     " << Utils::format_duration(total_scan_time_us.load()) << std::endl;
    
    // Cache statistics (if used)
    size_t total_cache_ops = cache_hits.load() + cache_misses.load();
    if (total_cache_ops > 0) {
        oss << "\n[Cache Statistics]" << std::endl;
        oss << "  Hits:                " << cache_hits.load() << std::endl;
        oss << "  Misses:              " << cache_misses.load() << std::endl;
        oss << "  Hit ratio:           " << std::fixed << std::setprecision(2) 
            << get_cache_hit_ratio() << "%" << std::endl;
    }
    
    // Process operations
    if (process_lookups.load() > 0) {
        oss << "\n[Process Operations]" << std::endl;
        oss << "  Lookups:             " << process_lookups.load() << std::endl;
        oss << "  Successful:          " << successful_process_lookups.load() << std::endl;
    }
    
    oss << "\n===============================" << std::endl;
    
    return oss.str();
}

std::string DMAMetrics::to_summary_string() const {
    std::ostringstream oss;
    
    oss << "DMA Metrics: ";
    oss << total_read_operations.load() << " reads (";
    oss << std::fixed << std::setprecision(1) << get_success_rate() << "% success), ";
    oss << Utils::format_bytes(total_bytes_read.load()) << " @ ";
    oss << std::fixed << std::setprecision(2) << get_throughput_mbps() << " MB/s, ";
    oss << total_signatures_scanned.load() << " scans (";
    oss << signatures_found.load() << " found)";
    
    return oss.str();
}

// ============================================================================
// MetricsCollector Implementation
// ============================================================================

MetricsCollector::MetricsCollector() : enabled_(true) {}

void MetricsCollector::record_read(size_t bytes, uint64_t duration_us, bool success, bool partial) {
    if (!enabled_.load()) return;
    
    metrics_.total_read_operations.fetch_add(1);
    
    if (success) {
        metrics_.total_bytes_read.fetch_add(bytes);
        metrics_.total_read_time_us.fetch_add(duration_us);
        
        if (partial) {
            metrics_.partial_read_operations.fetch_add(1);
        }
        
        // Update min/max times
        update_min_time(duration_us);
        update_max_time(duration_us);
    } else {
        metrics_.failed_read_operations.fetch_add(1);
    }
}

void MetricsCollector::record_scan(size_t bytes_scanned, uint64_t duration_us, bool found) {
    if (!enabled_.load()) return;
    
    metrics_.total_signatures_scanned.fetch_add(1);
    metrics_.total_scan_bytes.fetch_add(bytes_scanned);
    metrics_.total_scan_time_us.fetch_add(duration_us);
    
    if (found) {
        metrics_.signatures_found.fetch_add(1);
    } else {
        metrics_.signatures_not_found.fetch_add(1);
    }
}

void MetricsCollector::record_cache_hit() {
    if (!enabled_.load()) return;
    metrics_.cache_hits.fetch_add(1);
}

void MetricsCollector::record_cache_miss() {
    if (!enabled_.load()) return;
    metrics_.cache_misses.fetch_add(1);
}

void MetricsCollector::record_process_lookup(bool success) {
    if (!enabled_.load()) return;
    
    metrics_.process_lookups.fetch_add(1);
    if (success) {
        metrics_.successful_process_lookups.fetch_add(1);
    }
}

const DMAMetrics& MetricsCollector::get_metrics() const {
    return metrics_;
}

void MetricsCollector::reset_metrics() {
    metrics_.reset();
}

void MetricsCollector::enable() {
    enabled_.store(true);
}

void MetricsCollector::disable() {
    enabled_.store(false);
}

bool MetricsCollector::is_enabled() const {
    return enabled_.load();
}

void MetricsCollector::log_summary() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout << metrics_.to_summary_string() << std::endl;
}

void MetricsCollector::log_detailed() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout << metrics_.to_string() << std::endl;
}

void MetricsCollector::update_min_time(uint64_t new_time) {
    uint64_t current_min = metrics_.min_read_time_us.load();
    while (new_time < current_min) {
        if (metrics_.min_read_time_us.compare_exchange_weak(current_min, new_time)) {
            break;
        }
    }
}

void MetricsCollector::update_max_time(uint64_t new_time) {
    uint64_t current_max = metrics_.max_read_time_us.load();
    while (new_time > current_max) {
        if (metrics_.max_read_time_us.compare_exchange_weak(current_max, new_time)) {
            break;
        }
    }
}

// ============================================================================
// ScopedTimer Implementation
// ============================================================================

ScopedTimer::ScopedTimer() : start_time_(Clock::now()) {}

uint64_t ScopedTimer::elapsed_us() const {
    auto end_time = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
    return static_cast<uint64_t>(duration.count());
}

void ScopedTimer::reset() {
    start_time_ = Clock::now();
}

// ============================================================================
// Utility Functions
// ============================================================================

namespace Utils {

std::string format_duration(uint64_t microseconds) {
    std::ostringstream oss;
    
    if (microseconds < 1000) {
        // Less than 1ms - show in microseconds
        oss << microseconds << " μs";
    } else if (microseconds < 1000000) {
        // Less than 1s - show in milliseconds
        double ms = static_cast<double>(microseconds) / 1000.0;
        oss << std::fixed << std::setprecision(2) << ms << " ms";
    } else {
        // Show in seconds
        double seconds = us_to_seconds(microseconds);
        oss << std::fixed << std::setprecision(2) << seconds << " s";
    }
    
    return oss.str();
}

std::string format_bytes(size_t bytes) {
    std::ostringstream oss;
    
    if (bytes < 1024) {
        oss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        double kb = static_cast<double>(bytes) / 1024.0;
        oss << std::fixed << std::setprecision(2) << kb << " KB";
    } else if (bytes < 1024ULL * 1024 * 1024) {
        double mb = bytes_to_mb(bytes);
        oss << std::fixed << std::setprecision(2) << mb << " MB";
    } else {
        double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        oss << std::fixed << std::setprecision(2) << gb << " GB";
    }
    
    return oss.str();
}

} // namespace Utils

} // namespace Metrics
} // namespace VolkDMA
