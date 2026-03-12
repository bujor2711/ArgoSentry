// ArgoSentry Enhanced Pattern Scanner v3.1
// IDA-style pattern scanning with wildcards for signature detection
// Part of Phase 3.1: Reverse Engineering Tools

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <map>
#include <mutex>
#include <memory>

namespace ArgoSentry {

// Forward declarations
class DMA;

/**
 * @brief Compiled pattern for efficient scanning
 * @details Pre-processes IDA-style patterns for fast matching
 */
struct CompiledPattern {
    std::vector<uint8_t> bytes;        ///< Pattern bytes
    std::vector<bool> mask;            ///< true = match, false = wildcard
    std::string original;              ///< Original pattern string
    size_t pattern_size;               ///< Size in bytes
    
    CompiledPattern() : pattern_size(0) {}
};

/**
 * @brief Pattern match result
 */
struct PatternMatch {
    uint64_t address;                  ///< Match address
    std::string pattern_id;            ///< Pattern identifier (for multi-pattern)
    size_t match_index;                ///< Match index (0 = first, 1 = second, etc.)
    
    PatternMatch(uint64_t addr, const std::string& id = "", size_t idx = 0)
        : address(addr), pattern_id(id), match_index(idx) {}
};

/**
 * @brief Statistics for pattern scanning operations
 */
struct PatternScanStats {
    size_t total_scans{0};             ///< Total scan operations
    size_t total_matches{0};           ///< Total matches found
    size_t cached_patterns{0};         ///< Compiled patterns in cache
    size_t bytes_scanned{0};           ///< Total bytes scanned
    double average_scan_time_ms{0.0};  ///< Average scan time
    
    [[nodiscard]] double get_match_rate() const noexcept {
        return total_scans > 0 ? (total_matches * 100.0) / total_scans : 0.0;
    }
};

/**
 * @brief Enhanced pattern scanner with IDA-style syntax
 * @details Supports wildcards, multi-pattern scanning, and caching
 * @since v3.1
 * 
 * Example:
 * @code
 * EnhancedPatternScanner scanner(dma, pid);
 * 
 * // IDA-style pattern with wildcards
 * auto results = scanner.scan_pattern(
 *     "48 8B 05 ?? ?? ?? ??",  // MOV RAX, [RIP+??]
 *     start_addr,
 *     end_addr
 * );
 * 
 * // Multi-pattern scan (find any of these)
 * auto match = scanner.scan_multi_patterns({
 *     "48 8B 05 ?? ?? ?? ??",  // x64 pattern
 *     "8B 05 ?? ?? ?? ??",     // x86 pattern
 * }, start_addr, end_addr, true); // true = first match only
 * @endcode
 */
class EnhancedPatternScanner {
public:
    /**
     * @brief Construct enhanced pattern scanner
     * @param dma DMA instance for memory operations
     * @param process_id Target process ID
     */
    explicit EnhancedPatternScanner(DMA* dma, DWORD process_id);
    
    /**
     * @brief Destructor
     */
    ~EnhancedPatternScanner() = default;
    
    // Disable copy/move
    EnhancedPatternScanner(const EnhancedPatternScanner&) = delete;
    EnhancedPatternScanner& operator=(const EnhancedPatternScanner&) = delete;
    EnhancedPatternScanner(EnhancedPatternScanner&&) = delete;
    EnhancedPatternScanner& operator=(EnhancedPatternScanner&&) = delete;
    
    /**
     * @brief Scan for IDA-style pattern
     * @param pattern Pattern string (e.g., "48 8B 05 ?? ?? ?? ??")
     * @param start_address Start of scan region
     * @param end_address End of scan region
     * @param first_match_only Stop after first match (optimization)
     * @return Vector of matches (addresses)
     * 
     * Pattern format:
     * - "48 8B 05" = exact bytes (hex)
     * - "??" = wildcard (any byte)
     * - Spaces are optional
     * - Case insensitive
     */
    [[nodiscard]] std::vector<uint64_t> scan_pattern(
        const std::string& pattern,
        uint64_t start_address,
        uint64_t end_address,
        bool first_match_only = false
    );
    
    /**
     * @brief Scan for multiple patterns (find any match)
     * @param patterns Vector of pattern strings
     * @param start_address Start of scan region
     * @param end_address End of scan region
     * @param first_match_only Stop after first match of any pattern
     * @return Vector of matches with pattern IDs
     */
    [[nodiscard]] std::vector<PatternMatch> scan_multi_patterns(
        const std::vector<std::string>& patterns,
        uint64_t start_address,
        uint64_t end_address,
        bool first_match_only = false
    );
    
    /**
     * @brief Compile pattern for reuse (caching)
     * @param pattern Pattern string
     * @param pattern_id Optional identifier for cache
     * @return Compiled pattern
     */
    [[nodiscard]] CompiledPattern compile_pattern(
        const std::string& pattern,
        const std::string& pattern_id = ""
    );
    
    /**
     * @brief Scan with pre-compiled pattern
     * @param compiled Pre-compiled pattern
     * @param start_address Start of scan region
     * @param end_address End of scan region
     * @param first_match_only Stop after first match
     * @return Vector of match addresses
     */
    [[nodiscard]] std::vector<uint64_t> scan_compiled(
        const CompiledPattern& compiled,
        uint64_t start_address,
        uint64_t end_address,
        bool first_match_only = false
    );
    
    /**
     * @brief Clear pattern cache
     */
    void clear_cache() noexcept;
    
    /**
     * @brief Get scanner statistics
     */
    [[nodiscard]] PatternScanStats get_stats() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    /**
     * @brief Reset statistics
     */
    void reset_stats() noexcept;
    
    /**
     * @brief Check if pattern is in cache
     * @param pattern_id Pattern identifier
     */
    [[nodiscard]] bool is_cached(const std::string& pattern_id) const;
    
    /**
     * @brief Get cached pattern
     * @param pattern_id Pattern identifier
     * @return Compiled pattern if found
     */
    [[nodiscard]] std::optional<CompiledPattern> get_cached_pattern(
        const std::string& pattern_id
    ) const;

private:
    DMA* dma_;                                           ///< DMA instance
    DWORD process_id_;                                   ///< Target process
    
    std::map<std::string, CompiledPattern> pattern_cache_; ///< Compiled patterns
    mutable std::mutex mutex_;                           ///< Thread safety
    
    PatternScanStats stats_;                             ///< Statistics
    
    /**
     * @brief Parse IDA-style pattern string
     * @param pattern Pattern string with wildcards
     * @return Compiled pattern
     */
    CompiledPattern parse_ida_pattern(const std::string& pattern);
    
    /**
     * @brief Match pattern against memory buffer
     * @param buffer Memory buffer
     * @param buffer_size Size of buffer
     * @param compiled Compiled pattern
     * @param matches Output vector for match offsets
     * @param first_match_only Stop after first match
     */
    void match_pattern_in_buffer(
        const uint8_t* buffer,
        size_t buffer_size,
        const CompiledPattern& compiled,
        std::vector<size_t>& matches,
        bool first_match_only
    ) const;
    
    /**
     * @brief Generate cache key from pattern
     */
    std::string generate_cache_key(const std::string& pattern) const;
};

} // namespace ArgoSentry
