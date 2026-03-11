#include "include/VolkDMA/cache.hh"
#include <algorithm>
#include <iostream>

namespace VolkDMA {
namespace Cache {

MemoryCache::MemoryCache(size_t max_size, std::chrono::seconds time_to_live)
    : max_cache_size_(max_size)
    , ttl_(time_to_live)
    , enabled_(true)
    , current_size_(0)
    , hits_(0)
    , misses_(0)
    , evictions_(0) {
}

std::optional<std::vector<uint8_t>> MemoryCache::get(uint64_t addr, size_t size) {
    if (!enabled_) {
        return std::nullopt;
    }
    
    // Create cache key (address + size)
    uint64_t key = make_cache_key(addr, size);
    
    // Try to find in cache (shared lock for reading)
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Check if expired
            if (is_expired(it->second)) {
                misses_++;
                return std::nullopt;
            }
            
            // Cache hit!
            hits_++;
            it->second.hit_count++;
            return it->second.data;
        }
    }
    
    // Cache miss
    misses_++;
    return std::nullopt;
}

void MemoryCache::put(uint64_t addr, const std::vector<uint8_t>& data) {
    if (!enabled_) {
        return;
    }
    
    size_t data_size = data.size();
    uint64_t key = make_cache_key(addr, data_size);
    
    // Exclusive lock for writing
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    // Check if entry already exists
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // Update existing entry
        current_size_ -= it->second.data.size();
        it->second.data = data;
        it->second.timestamp = std::chrono::steady_clock::now();
        it->second.hit_count = 0;
        current_size_ += data_size;
        return;
    }
    
    // Evict until we have space
    while (current_size_ + data_size > max_cache_size_ && !cache_.empty()) {
        evict_lru();
    }
    
    // Don't cache if single entry is too large
    if (data_size > max_cache_size_) {
        return;
    }
    
    // Insert new entry
    cache_.emplace(key, CacheEntry(addr, data));
    current_size_ += data_size;
}

void MemoryCache::invalidate(uint64_t addr) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    // Find all entries with this base address
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->second.address == addr) {
            current_size_ -= it->second.data.size();
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void MemoryCache::clear() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    cache_.clear();
    current_size_ = 0;
}

void MemoryCache::evict_expired() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (is_expired(it->second)) {
            current_size_ -= it->second.data.size();
            evictions_++;
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t MemoryCache::get_size() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return current_size_;
}

size_t MemoryCache::get_entry_count() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_.size();
}

MemoryCache::Statistics MemoryCache::get_statistics() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    Statistics stats;
    stats.hits = hits_;
    stats.misses = misses_;
    stats.evictions = evictions_;
    stats.current_size = current_size_;
    stats.entry_count = cache_.size();
    
    size_t total = hits_ + misses_;
    stats.hit_rate = total > 0 ? (static_cast<double>(hits_) / total * 100.0) : 0.0;
    
    return stats;
}

void MemoryCache::reset_statistics() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    hits_ = 0;
    misses_ = 0;
    evictions_ = 0;
}

void MemoryCache::set_enabled(bool enabled) {
    enabled_ = enabled;
}

bool MemoryCache::is_enabled() const {
    return enabled_;
}

// Private helper functions

void MemoryCache::evict_lru() {
    // Find entry with lowest hit count (LRU approximation)
    auto lru_it = std::min_element(cache_.begin(), cache_.end(),
        [](const auto& a, const auto& b) {
            // Compare by hit count first, then by timestamp
            if (a.second.hit_count != b.second.hit_count) {
                return a.second.hit_count < b.second.hit_count;
            }
            return a.second.timestamp < b.second.timestamp;
        });
    
    if (lru_it != cache_.end()) {
        current_size_ -= lru_it->second.data.size();
        evictions_++;
        cache_.erase(lru_it);
    }
}

bool MemoryCache::is_expired(const CacheEntry& entry) const {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.timestamp);
    return age > ttl_;
}

uint64_t MemoryCache::make_cache_key(uint64_t addr, size_t size) const {
    // Simple key: combine address and size
    // For better distribution, we could use a hash function
    return addr ^ (static_cast<uint64_t>(size) << 32);
}

// v2.2 - New configuration methods

void MemoryCache::set_max_size(size_t size) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    max_cache_size_ = size;

    // If new size is smaller, evict until we fit
    while (current_size_ > max_cache_size_ && !cache_.empty()) {
        evict_lru();
    }
}

void MemoryCache::set_ttl(std::chrono::seconds ttl) {
    ttl_ = ttl;
    // Evict any entries that are now expired with new TTL
    evict_expired();
}

size_t MemoryCache::get_max_size() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return max_cache_size_;
}

std::chrono::seconds MemoryCache::get_ttl() const {
    return ttl_;
}

} // namespace Cache
} // namespace VolkDMA
