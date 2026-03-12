// value_freezer.cpp - Value Freezer Implementation
// Maintains constant memory values with background worker thread

#include "../include/ArgoSentry/value_freezer.hh"
#include "../include/ArgoSentry/dma.hh"
#include <algorithm>

namespace ArgoSentry {

ValueFreezer::ValueFreezer(DMA* dma, DWORD process_id)
    : dma_(dma)
    , process_id_(process_id)
{
    if (!dma_) {
        throw std::invalid_argument("DMA instance cannot be null");
    }
    
    // Start worker thread
    worker_running_.store(true, std::memory_order_relaxed);
    worker_thread_ = std::make_unique<std::thread>(&ValueFreezer::worker_loop, this);
}

ValueFreezer::~ValueFreezer() {
    // Stop worker thread
    worker_running_.store(false, std::memory_order_relaxed);
    
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
}

bool ValueFreezer::unfreeze(uint64_t address) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t removed = frozen_values_.erase(address);
    stats_.active_frozen_values = frozen_values_.size();
    return removed > 0;
}

void ValueFreezer::unfreeze_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    frozen_values_.clear();
    stats_.active_frozen_values = 0;
}

void ValueFreezer::pause_all() noexcept {
    global_paused_.store(true, std::memory_order_relaxed);
}

void ValueFreezer::resume_all() noexcept {
    global_paused_.store(false, std::memory_order_relaxed);
}

bool ValueFreezer::pause(uint64_t address) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = frozen_values_.find(address);
    if (it != frozen_values_.end()) {
        it->second.paused = true;
        
        // Count paused values
        stats_.paused_values = 0;
        for (const auto& [addr, frozen] : frozen_values_) {
            if (frozen.paused) {
                stats_.paused_values++;
            }
        }
        return true;
    }
    return false;
}

bool ValueFreezer::resume(uint64_t address) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = frozen_values_.find(address);
    if (it != frozen_values_.end()) {
        it->second.paused = false;
        
        // Count paused values
        stats_.paused_values = 0;
        for (const auto& [addr, frozen] : frozen_values_) {
            if (frozen.paused) {
                stats_.paused_values++;
            }
        }
        return true;
    }
    return false;
}

bool ValueFreezer::is_frozen(uint64_t address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frozen_values_.find(address) != frozen_values_.end();
}

bool ValueFreezer::is_paused(uint64_t address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = frozen_values_.find(address);
    return it != frozen_values_.end() && it->second.paused;
}

std::vector<uint64_t> ValueFreezer::get_frozen_addresses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint64_t> addresses;
    addresses.reserve(frozen_values_.size());
    
    for (const auto& [address, _] : frozen_values_) {
        addresses.push_back(address);
    }
    
    return addresses;
}

void ValueFreezer::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.total_writes = 0;
    stats_.failed_writes = 0;
    stats_.start_time = std::chrono::steady_clock::now();
}

void ValueFreezer::worker_loop() {
    while (worker_running_.load(std::memory_order_relaxed)) {
        // Check global pause
        if (global_paused_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        // Copy frozen values to avoid holding lock too long
        std::map<uint64_t, FrozenValue> values_to_write;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            values_to_write = frozen_values_;
        }
        
        // Write each value if interval has elapsed
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [address, frozen] : values_to_write) {
            // Skip if paused
            if (frozen.paused) {
                continue;
            }
            
            // Check if interval has elapsed
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - frozen.last_write).count();
            
            if (elapsed >= frozen.interval_ms) {
                // Write value
                bool success = write_frozen_value(address, frozen);
                
                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.total_writes++;
                    if (!success) {
                        stats_.failed_writes++;
                    }
                    
                    // Update last write time
                    auto it = frozen_values_.find(address);
                    if (it != frozen_values_.end()) {
                        it->second.last_write = now;
                    }
                }
            }
        }
        
        // Sleep for a bit (10ms minimum tick)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool ValueFreezer::write_frozen_value(uint64_t address, const FrozenValue& frozen) {
    if (!dma_ || frozen.value_bytes.empty()) {
        return false;
    }
    
    // Write raw bytes to memory
    auto result = dma_->write_raw(address, frozen.value_bytes.data(), 
                                  frozen.value_bytes.size(), process_id_);
    
    return result.has_value() && result.value();
}

} // namespace ArgoSentry
