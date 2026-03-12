// ArgoSentry Pointer Chain Resolver v3.1
// Automatic resolution of pointer chains for RE and game hacking
// Part of Phase 3.1: Reverse Engineering Tools

#pragma once

#include <vector>
#include <optional>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <functional>

namespace ArgoSentry {

// Forward declarations
class DMA;

/**
 * @brief Represents a pointer chain (multi-level pointer)
 * @details Resolves chains like [[base + 0x10] + 0x20] + 0x30
 * @since v3.1
 * 
 * Example:
 *   PointerChain chain({0x140000000, 0x10, 0x20, 0x08});
 *   uint64_t final_address = chain.resolve(dma, pid);
 */
class PointerChain {
public:
    /**
     * @brief Construct a pointer chain from base address + offsets
     * @param base_address Starting address (can be module base + offset)
     * @param offsets Vector of offsets to traverse
     */
    explicit PointerChain(uint64_t base_address, const std::vector<uint64_t>& offsets = {});
    
    /**
     * @brief Construct from full chain (base is first element)
     * @param chain_addresses Full chain including base address
     */
    explicit PointerChain(const std::vector<uint64_t>& chain_addresses);
    
    /**
     * @brief Resolve the pointer chain to final address
     * @param dma DMA instance for memory reading
     * @param process_id Target process ID
     * @return Final address if successful, std::nullopt if chain is broken
     */
    [[nodiscard]] std::optional<uint64_t> resolve(DMA& dma, DWORD process_id);
    
    /**
     * @brief Resolve with custom read function
     * @param read_func Custom function for reading pointers
     * @return Final address if successful
     */
    [[nodiscard]] std::optional<uint64_t> resolve(
        std::function<std::optional<uint64_t>(uint64_t)> read_func
    );
    
    /**
     * @brief Validate entire chain (check all pointers are valid)
     * @param dma DMA instance
     * @param process_id Target process ID
     * @return true if all pointers in chain are valid
     */
    [[nodiscard]] bool validate(DMA& dma, DWORD process_id);
    
    /**
     * @brief Get the depth of the chain (number of dereferences)
     * @return Number of pointer dereferences (offsets count)
     */
    [[nodiscard]] size_t depth() const noexcept { return offsets_.size(); }
    
    /**
     * @brief Check if chain is empty
     */
    [[nodiscard]] bool empty() const noexcept { return offsets_.empty(); }
    
    /**
     * @brief Get base address
     */
    [[nodiscard]] uint64_t base_address() const noexcept { return base_address_; }
    
    /**
     * @brief Get all offsets
     */
    [[nodiscard]] const std::vector<uint64_t>& offsets() const noexcept { return offsets_; }
    
    /**
     * @brief Add an offset to the chain
     */
    void add_offset(uint64_t offset);
    
    /**
     * @brief Set base address
     */
    void set_base_address(uint64_t base) noexcept { base_address_ = base; }
    
    /**
     * @brief Clear all offsets (keep base address)
     */
    void clear_offsets() noexcept { offsets_.clear(); }
    
    /**
     * @brief Enable/disable caching of resolved addresses
     * @param enable Enable caching
     * @param ttl_ms Cache time-to-live in milliseconds (default: 1000ms)
     */
    void enable_cache(bool enable, uint32_t ttl_ms = 1000);
    
    /**
     * @brief Invalidate cache (force re-resolve on next call)
     */
    void invalidate_cache() noexcept;
    
    /**
     * @brief Get string representation for debugging
     * @return String like "[0x140000000] -> [+0x10] -> [+0x20] -> [+0x08]"
     */
    [[nodiscard]] std::string to_string() const;
    
    /**
     * @brief Parse pointer chain from string format
     * @param chain_str String like "0x140000000+0x10+0x20+0x08"
     * @return PointerChain if parsing successful
     */
    [[nodiscard]] static std::optional<PointerChain> from_string(const std::string& chain_str);

private:
    uint64_t base_address_;                  ///< Starting address
    std::vector<uint64_t> offsets_;          ///< Offsets to traverse
    
    // Caching
    bool cache_enabled_{false};              ///< Enable address caching
    uint32_t cache_ttl_ms_{1000};           ///< Cache time-to-live
    std::optional<uint64_t> cached_address_; ///< Cached resolved address
    std::chrono::steady_clock::time_point cache_timestamp_; ///< When cache was updated
    mutable std::mutex cache_mutex_;         ///< Protect cache access
    
    // Helper methods
    [[nodiscard]] bool is_cache_valid() const noexcept;
    void update_cache(uint64_t address) noexcept;
};

/**
 * @brief Manager for multiple pointer chains with names
 * @details Useful for managing multiple pointers in a game
 * @since v3.1
 */
class PointerChainManager {
public:
    /**
     * @brief Add a named pointer chain
     * @param name Identifier for the chain (e.g., "player_health")
     * @param chain The pointer chain
     */
    void add_chain(const std::string& name, const PointerChain& chain);
    
    /**
     * @brief Get a pointer chain by name
     * @param name Chain identifier
     * @return Chain if found
     */
    [[nodiscard]] std::optional<PointerChain> get_chain(const std::string& name) const;
    
    /**
     * @brief Remove a chain
     */
    bool remove_chain(const std::string& name);
    
    /**
     * @brief Resolve a chain by name
     * @param name Chain identifier
     * @param dma DMA instance
     * @param process_id Target process
     * @return Resolved address if successful
     */
    [[nodiscard]] std::optional<uint64_t> resolve(
        const std::string& name,
        DMA& dma,
        DWORD process_id
    );
    
    /**
     * @brief Resolve all chains
     * @return Map of name -> resolved address
     */
    [[nodiscard]] std::map<std::string, std::optional<uint64_t>> resolve_all(
        DMA& dma,
        DWORD process_id
    );
    
    /**
     * @brief Validate all chains
     * @return Map of name -> validation result
     */
    [[nodiscard]] std::map<std::string, bool> validate_all(
        DMA& dma,
        DWORD process_id
    );
    
    /**
     * @brief Get all chain names
     */
    [[nodiscard]] std::vector<std::string> get_chain_names() const;
    
    /**
     * @brief Clear all chains
     */
    void clear() noexcept;
    
    /**
     * @brief Get number of chains
     */
    [[nodiscard]] size_t size() const noexcept { return chains_.size(); }
    
    /**
     * @brief Save chains to JSON file
     * @param filepath Path to JSON file
     * @return true if successful
     */
    bool save_to_file(const std::string& filepath) const;
    
    /**
     * @brief Load chains from JSON file
     * @param filepath Path to JSON file
     * @return true if successful
     */
    bool load_from_file(const std::string& filepath);

private:
    std::map<std::string, PointerChain> chains_;
    mutable std::mutex mutex_;
};

} // namespace ArgoSentry
