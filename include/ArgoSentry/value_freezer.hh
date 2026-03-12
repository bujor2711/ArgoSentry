// ArgoSentry Value Freezer v3.1
// Maintains constant memory values (god mode, infinite ammo, etc.)
// Part of Phase 3.1: Reverse Engineering Tools

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <functional>
#include <vector>

namespace ArgoSentry {

// Forward declarations
class DMA;

/**
 * @brief Represents a frozen memory value
 * @details Stores value, type info, and freeze interval
 */
struct FrozenValue {
    std::vector<uint8_t> value_bytes;      ///< Raw bytes of the value
    size_t value_size;                     ///< Size in bytes
    uint32_t interval_ms;                  ///< Write interval in milliseconds
    std::chrono::steady_clock::time_point last_write; ///< Last write timestamp
    bool paused{false};                    ///< Temporarily paused?
    
    FrozenValue() : value_size(0), interval_ms(100) {}
    
    template<typename T>
    FrozenValue(T value, uint32_t interval) 
        : value_size(sizeof(T))
        , interval_ms(interval)
        , last_write(std::chrono::steady_clock::now())
    {
        value_bytes.resize(sizeof(T));
        std::memcpy(value_bytes.data(), &value, sizeof(T));
    }
};

/**
 * @brief Statistics for value freezing operations
 */
struct ValueFreezerStats {
    size_t active_frozen_values{0};        ///< Currently frozen values
    size_t total_writes{0};                ///< Total write operations
    size_t failed_writes{0};               ///< Failed write operations
    size_t paused_values{0};               ///< Currently paused values
    std::chrono::steady_clock::time_point start_time; ///< When worker started
    
    ValueFreezerStats() : start_time(std::chrono::steady_clock::now()) {}
    
    [[nodiscard]] double get_uptime_seconds() const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        return static_cast<double>(elapsed.count());
    }
    
    [[nodiscard]] double get_success_rate() const noexcept {
        if (total_writes == 0) return 100.0;
        return ((total_writes - failed_writes) * 100.0) / total_writes;
    }
};

/**
 * @brief Automatically maintains constant memory values
 * @details Background worker thread continuously writes frozen values
 * @since v3.1
 * 
 * Example:
 * @code
 * ValueFreezer freezer(dma, pid);
 * 
 * // Freeze player health at 100
 * freezer.freeze_value<int32_t>(health_addr, 100, 50); // Write every 50ms
 * 
 * // Freeze ammo at 999
 * freezer.freeze_value<int32_t>(ammo_addr, 999, 100);
 * 
 * // Pause all freezing temporarily
 * freezer.pause_all();
 * // ... do something ...
 * freezer.resume_all();
 * 
 * // Unfreeze specific value
 * freezer.unfreeze(health_addr);
 * @endcode
 */
class ValueFreezer {
public:
    /**
     * @brief Construct value freezer
     * @param dma DMA instance for memory operations
     * @param process_id Target process ID
     */
    explicit ValueFreezer(DMA* dma, DWORD process_id);
    
    /**
     * @brief Destructor - stops worker thread
     */
    ~ValueFreezer();
    
    // Disable copy/move
    ValueFreezer(const ValueFreezer&) = delete;
    ValueFreezer& operator=(const ValueFreezer&) = delete;
    ValueFreezer(ValueFreezer&&) = delete;
    ValueFreezer& operator=(ValueFreezer&&) = delete;
    
    /**
     * @brief Freeze a value at specified address
     * @tparam T Value type (int32_t, float, uint64_t, etc.)
     * @param address Memory address to freeze
     * @param value Value to maintain
     * @param interval_ms Write interval in milliseconds (default: 100ms)
     */
    template<typename T>
    void freeze_value(uint64_t address, T value, uint32_t interval_ms = 100) {
        std::lock_guard<std::mutex> lock(mutex_);
        frozen_values_[address] = FrozenValue(value, interval_ms);
        stats_.active_frozen_values = frozen_values_.size();
    }
    
    /**
     * @brief Unfreeze a specific address
     * @param address Address to unfreeze
     * @return true if value was frozen
     */
    bool unfreeze(uint64_t address);
    
    /**
     * @brief Unfreeze all values and stop worker
     */
    void unfreeze_all();
    
    /**
     * @brief Temporarily pause all freezing (don't write, keep values)
     */
    void pause_all() noexcept;
    
    /**
     * @brief Resume all freezing
     */
    void resume_all() noexcept;
    
    /**
     * @brief Pause specific address
     * @param address Address to pause
     * @return true if value exists
     */
    bool pause(uint64_t address);
    
    /**
     * @brief Resume specific address
     * @param address Address to resume
     * @return true if value exists
     */
    bool resume(uint64_t address);
    
    /**
     * @brief Check if value is frozen
     * @param address Address to check
     * @return true if currently frozen
     */
    [[nodiscard]] bool is_frozen(uint64_t address) const;
    
    /**
     * @brief Check if value is paused
     * @param address Address to check
     * @return true if paused
     */
    [[nodiscard]] bool is_paused(uint64_t address) const;
    
    /**
     * @brief Get number of frozen values
     */
    [[nodiscard]] size_t get_frozen_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return frozen_values_.size();
    }
    
    /**
     * @brief Get all frozen addresses
     */
    [[nodiscard]] std::vector<uint64_t> get_frozen_addresses() const;
    
    /**
     * @brief Get freezer statistics
     */
    [[nodiscard]] ValueFreezerStats get_stats() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    /**
     * @brief Reset statistics
     */
    void reset_stats() noexcept;
    
    /**
     * @brief Check if worker is running
     */
    [[nodiscard]] bool is_running() const noexcept {
        return worker_running_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Set global pause state
     * @param paused true to pause all, false to resume all
     */
    void set_global_pause(bool paused) noexcept {
        global_paused_.store(paused, std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if globally paused
     */
    [[nodiscard]] bool is_global_paused() const noexcept {
        return global_paused_.load(std::memory_order_relaxed);
    }

private:
    DMA* dma_;                                     ///< DMA instance
    DWORD process_id_;                             ///< Target process
    
    std::map<uint64_t, FrozenValue> frozen_values_; ///< Address -> frozen value
    mutable std::mutex mutex_;                     ///< Protect frozen_values_
    
    std::atomic<bool> worker_running_{false};      ///< Worker thread running?
    std::atomic<bool> global_paused_{false};       ///< Global pause flag
    std::unique_ptr<std::thread> worker_thread_;   ///< Background worker
    
    ValueFreezerStats stats_;                      ///< Statistics
    
    /**
     * @brief Worker thread function - continuously writes frozen values
     */
    void worker_loop();
    
    /**
     * @brief Write a frozen value to memory
     * @param address Target address
     * @param frozen Frozen value data
     * @return true if write successful
     */
    bool write_frozen_value(uint64_t address, const FrozenValue& frozen);
};

} // namespace ArgoSentry
