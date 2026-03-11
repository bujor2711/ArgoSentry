// VolkDMA - Health Monitoring Integration with DMA Class
// v1.8 - Integrate health monitoring into DMA

#include "ArgoSentry/dma.hh"
#include "ArgoSentry/health.hh"
#include <stdexcept>

namespace ArgoSentry {

//==============================================================================
// Health Monitoring Methods
//==============================================================================

void DMA::enable_health_monitoring(bool enable) {
    if (enable && !health_monitor_) {
        // Create health monitor
        health_monitor_ = std::make_unique<HealthMonitor>(this);
    } else if (!enable && health_monitor_) {
        // Destroy health monitor (stops monitoring automatically)
        health_monitor_.reset();
    }
}

bool DMA::is_health_monitoring_enabled() const {
    return health_monitor_ != nullptr;
}

void DMA::run_health_checks() {
    if (!health_monitor_) {
        throw std::runtime_error("Health monitoring not enabled. Call enable_health_monitoring(true) first.");
    }
    
    health_monitor_->run_all_checks();
}

HealthStatus DMA::get_health_status() const {
    if (!health_monitor_) {
        throw std::runtime_error("Health monitoring not enabled. Call enable_health_monitoring(true) first.");
    }
    
    return health_monitor_->get_overall_status();
}

std::string DMA::get_health_summary() const {
    if (!health_monitor_) {
        throw std::runtime_error("Health monitoring not enabled. Call enable_health_monitoring(true) first.");
    }
    
    return health_monitor_->get_health_summary();
}

void DMA::start_automatic_health_monitoring(std::chrono::seconds interval) {
    if (!health_monitor_) {
        // Auto-enable if not enabled
        enable_health_monitoring(true);
    }
    
    health_monitor_->start_monitoring(interval);
}

void DMA::stop_automatic_health_monitoring() {
    if (!health_monitor_) {
        return; // Nothing to stop
    }
    
    health_monitor_->stop_monitoring();
}

const HealthMonitor& DMA::get_health_monitor() const {
    if (!health_monitor_) {
        throw std::runtime_error("Health monitoring not enabled. Call enable_health_monitoring(true) first.");
    }
    
    return *health_monitor_;
}

} // namespace ArgoSentry


