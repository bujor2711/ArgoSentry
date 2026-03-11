// VolkDMA - Health Monitoring System
// v1.8 - FPGA and system health checks
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

namespace VolkDMA {

//==============================================================================
// Health Status
//==============================================================================

enum class HealthStatus {
    Healthy,    // All systems operational
    Degraded,   // Some issues but functional
    Unhealthy,  // Significant issues, may fail soon
    Critical    // System failure imminent or occurred
};

// Convert HealthStatus to string
inline const char* health_status_to_string(HealthStatus status) {
    switch (status) {
        case HealthStatus::Healthy:    return "Healthy";
        case HealthStatus::Degraded:   return "Degraded";
        case HealthStatus::Unhealthy:  return "Unhealthy";
        case HealthStatus::Critical:   return "Critical";
        default:                       return "Unknown";
    }
}

//==============================================================================
// Health Check Result
//==============================================================================

struct HealthCheck {
    std::string component;                               // Component name (e.g., "FPGA", "Driver")
    HealthStatus status;                                 // Health status
    std::string message;                                 // Descriptive message
    std::chrono::system_clock::time_point timestamp;     // When check was performed
    double value;                                        // Optional numeric value (e.g., error rate)
    
    HealthCheck()
        : status(HealthStatus::Healthy)
        , timestamp(std::chrono::system_clock::now())
        , value(0.0)
    {}
    
    HealthCheck(const std::string& comp, HealthStatus stat, const std::string& msg)
        : component(comp)
        , status(stat)
        , message(msg)
        , timestamp(std::chrono::system_clock::now())
        , value(0.0)
    {}
    
    HealthCheck(const std::string& comp, HealthStatus stat, const std::string& msg, double val)
        : component(comp)
        , status(stat)
        , message(msg)
        , timestamp(std::chrono::system_clock::now())
        , value(val)
    {}
};

//==============================================================================
// Health Monitor
//==============================================================================

class HealthMonitor {
public:
    // Constructor - requires DMA instance for checks
    explicit HealthMonitor(void* dma_instance);

    // Destructor - stops monitoring thread
    ~HealthMonitor();
    
    // Prevent copying
    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;
    
    //==========================================================================
    // Individual Health Checks
    //==========================================================================
    
    // Check if FPGA is responsive (test read/write)
    HealthCheck check_fpga_connection();
    
    // Check if memory mapping is valid
    HealthCheck check_memory_mapping();
    
    // Check driver status
    HealthCheck check_driver_status();
    
    // Check for performance degradation
    HealthCheck check_performance();
    
    // Check error rate (from metrics)
    HealthCheck check_error_rate();
    
    //==========================================================================
    // Aggregate Operations
    //==========================================================================
    
    // Run all health checks
    void run_all_checks();
    
    // Get overall health status (worst of all checks)
    HealthStatus get_overall_status() const;
    
    // Get recent health checks (last N checks)
    std::vector<HealthCheck> get_recent_checks(size_t count = 10) const;
    
    // Get current health issues (non-healthy checks)
    std::vector<std::string> get_health_issues() const;
    
    // Get health summary string
    std::string get_health_summary() const;
    
    //==========================================================================
    // Automatic Monitoring
    //==========================================================================
    
    // Start automatic monitoring (runs checks periodically)
    void start_monitoring(std::chrono::seconds interval = std::chrono::seconds(30));
    
    // Stop automatic monitoring
    void stop_monitoring();
    
    // Check if monitoring is active
    bool is_monitoring() const { return monitoring_active_.load(); }
    
    // Get monitoring interval
    std::chrono::seconds get_monitoring_interval() const { return check_interval_; }
    
    //==========================================================================
    // Configuration
    //==========================================================================
    
    // Set maximum checks to keep in history
    void set_max_history_size(size_t size) { max_history_size_ = size; }
    
    // Get maximum history size
    size_t get_max_history_size() const { return max_history_size_; }
    
    // Set error rate threshold (percentage, 0-100)
    void set_error_rate_threshold(double threshold) { error_rate_threshold_ = threshold; }
    
    // Get error rate threshold
    double get_error_rate_threshold() const { return error_rate_threshold_; }
    
    // Set performance degradation threshold (percentage drop, 0-100)
    void set_performance_threshold(double threshold) { performance_threshold_ = threshold; }
    
    // Get performance degradation threshold
    double get_performance_threshold() const { return performance_threshold_; }

private:
    // DMA instance for testing (stored as void* to avoid circular dependency)
    void* dma_;

    // Health check history
    std::vector<HealthCheck> check_history_;
    mutable std::mutex history_mutex_;
    
    // Configuration
    size_t max_history_size_;           // Max checks to keep
    double error_rate_threshold_;       // Error rate threshold (%)
    double performance_threshold_;      // Performance drop threshold (%)
    
    // Monitoring thread
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> monitoring_active_;
    std::chrono::seconds check_interval_;
    
    // Baseline performance (for degradation detection)
    double baseline_throughput_mbps_;
    mutable std::mutex baseline_mutex_;
    
    // Helper functions
    void add_check_to_history(const HealthCheck& check);
    void monitoring_loop();
    HealthStatus aggregate_status(const std::vector<HealthCheck>& checks) const;
};

} // namespace VolkDMA
