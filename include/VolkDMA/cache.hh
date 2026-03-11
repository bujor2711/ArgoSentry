#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <shared_mutex>
#include <optional>

namespace VolkDMA {
namespace Cache {

/**
 * @brief Cache entry containing data, timestamp, and statistics
 */
struct CacheEntry {
    uint64_t address;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;
    size_t hit_count;
    
    CacheEntry() : address(0), hit_count(0) {}
    
    CacheEntry(uint64_t addr, std::vector<uint8_t> d)
        : address(addr)
        , data(std::move(d))
        , timestamp(std::chrono::steady_clock::now())
        , hit_count(0) {}
};

/**
 * @brief Thread-safe LRU memory cache for DMA read operations
 * 
 * Features:
 * - LRU eviction when cache is full
 * - TTL (Time To Live) for preventing stale data
 * - Thread-safe with shared_mutex (multiple readers, single writer)
 * - Statistics tracking (hits, misses, evictions)
 * - Configurable size and TTL
 */
class MemoryCache {
public:
    /**
     * @brief Construct memory cache with size and TTL limits
     * @param max_size Maximum cache size in bytes (default: 100MB)
     * @param time_to_live Entry TTL in seconds (default: 30s)
     */
    explicit MemoryCache(size_t max_size = 100 * 1024 * 1024,
                        std::chrono::seconds time_to_live = std::chrono::seconds(30));
    
    /**
     * @brief Get cached data if available and not expired
     * @param addr Memory address
     * @param size Data size to retrieve
     * @return Optional containing data if cache hit, nullopt if miss
     */
    [[nodiscard]] std::optional<std::vector<uint8_t>> get(uint64_t addr, size_t size);
    
    /**
     * @brief Store data in cache
     * @param addr Memory address
     * @param data Data to cache
     */
    void put(uint64_t addr, const std::vector<uint8_t>& data);
    
    /**
     * @brief Invalidate specific address from cache
     * @param addr Memory address to invalidate
     */
    void invalidate(uint64_t addr);
    
    /**
     * @brief Clear entire cache
     */
    void clear();
    
    /**
     * @brief Remove expired entries based on TTL
     */
    void evict_expired();
    
    /**
     * @brief Get current cache size in bytes
     */
    [[nodiscard]] size_t get_size() const;
    
    /**
     * @brief Get number of entries in cache
     */
    [[nodiscard]] size_t get_entry_count() const;
    
    /**
     * @brief Get cache statistics
     */
    struct Statistics {
        size_t hits;
        size_t misses;
        size_t evictions;
        size_t current_size;
        size_t entry_count;
        double hit_rate;
    };
    
    [[nodiscard]] Statistics get_statistics() const;
    
    /**
     * @brief Reset statistics counters
     */
    void reset_statistics();
    
    /**
     * @brief Enable/disable cache
     */
    void set_enabled(bool enabled);
    
    /**
     * @brief Check if cache is enabled
     */
    [[nodiscard]] bool is_enabled() const;

    /**
     * @brief Set maximum cache size (v2.2)
     * @param size New maximum size in bytes
     */
    void set_max_size(size_t size);

    /**
     * @brief Set cache TTL (v2.2)
     * @param ttl New time-to-live duration
     */
    void set_ttl(std::chrono::seconds ttl);

    /**
     * @brief Get maximum cache size (v2.2)
     */
    [[nodiscard]] size_t get_max_size() const;

    /**
     * @brief Get cache TTL (v2.2)
     */
    [[nodiscard]] std::chrono::seconds get_ttl() const;

private:
    // Cache storage
    std::unordered_map<uint64_t, CacheEntry> cache_;
    mutable std::shared_mutex cache_mutex_;
    
    // Configuration
    size_t max_cache_size_;
    std::chrono::seconds ttl_;
    bool enabled_;
    
    // Current state
    size_t current_size_;
    
    // Statistics
    mutable size_t hits_;
    mutable size_t misses_;
    size_t evictions_;
    
    // Helper functions
    void evict_lru();
    bool is_expired(const CacheEntry& entry) const;
    uint64_t make_cache_key(uint64_t addr, size_t size) const;
};

} // namespace Cache
} // namespace VolkDMA
