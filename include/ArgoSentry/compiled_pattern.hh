// ArgoSentry/compiled_pattern.hh - Pre-compiled Pattern Optimization
// v2.5 - 2-3x speedup for frequently reused patterns
#ifndef ARGOSENTRY_COMPILED_PATTERN_HH
#define ARGOSENTRY_COMPILED_PATTERN_HH

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace ArgoSentry {

/**
 * @brief Pre-compiled signature pattern for fast repeated scanning
 * 
 * Compiles a signature string (e.g., "48 8B 0D ? ? ? ?") into optimized
 * binary format, eliminating parsing overhead on repeated scans.
 * 
 * Benefits:
 * - 2-3x speedup for patterns used 10+ times
 * - Zero parsing overhead in loops
 * - Thread-safe after compilation
 * - Cache-friendly (contiguous memory)
 * 
 * Best for:
 * - Multi-process scanning (same pattern, multiple PIDs)
 * - Monitoring loops (hotpath scanning)
 * - Pattern libraries (database of common patterns)
 * 
 * Example usage:
 * @code
 * // Compile once:
 * auto player_pattern = CompiledPattern::compile("48 8B 0D ? ? ? ? 48 85 C9");
 * 
 * // Reuse many times (fast!):
 * for (auto& process : processes) {
 *     uint64_t addr = dma.find_signature(player_pattern, start, end, process.pid);
 * }
 * @endcode
 */
class CompiledPattern {
public:
    /**
     * @brief Compile a signature string into binary format
     * 
     * Parses hex signature with wildcards and creates optimized representation.
     * 
     * @param signature Pattern string (e.g., "48 8B 0D ? ? ? ?")
     *                  - Hex bytes: 00-FF (case-insensitive)
     *                  - Wildcards: ? or ??
     *                  - Separator: space
     * 
     * @return Compiled pattern ready for fast scanning
     * 
     * @throws std::invalid_argument if:
     *         - Pattern is empty
     *         - Contains invalid hex digits
     *         - Contains non-hex/non-wildcard tokens
     * 
     * Examples:
     * @code
     * auto p1 = CompiledPattern::compile("48 8B 0D");           // 3 exact bytes
     * auto p2 = CompiledPattern::compile("E8 ? ? ? ?");        // 1 byte + 4 wildcards
     * auto p3 = CompiledPattern::compile("48 8B ? ? 48 85");   // Mixed
     * @endcode
     */
    [[nodiscard]] static CompiledPattern compile(const std::string& signature);

    /**
     * @brief Fast pattern matching in memory buffer
     * 
     * Searches for pattern in memory buffer using pre-compiled bytes and mask.
     * 
     * @param data Memory buffer to search
     * @param size Buffer size in bytes
     * @param base_addr Base address for offset calculation
     * 
     * @return Address of first match (base_addr + offset), or 0 if not found
     * 
     * @note Thread-safe: Can be called from multiple threads concurrently
     * @note No allocations: Uses only pre-compiled data
     * 
     * Performance:
     * - Best case: O(n) where n = buffer size
     * - Average: O(n) with early break on mismatch
     * - No parsing overhead (already compiled)
     */
    [[nodiscard]] uint64_t find_in_buffer(
        const uint8_t* data, 
        size_t size, 
        uint64_t base_addr
    ) const;

    /**
     * @brief Get pattern length in bytes
     * @return Number of bytes in pattern
     */
    [[nodiscard]] size_t get_length() const noexcept { 
        return length_; 
    }

    /**
     * @brief Get pattern bytes (for debugging/inspection)
     * @return Vector of pattern bytes (wildcards are 0x00)
     */
    [[nodiscard]] const std::vector<uint8_t>& get_bytes() const noexcept { 
        return bytes_; 
    }

    /**
     * @brief Get pattern mask (for debugging/inspection)
     * @return Vector of mask bytes (0xFF = exact match, 0x00 = wildcard)
     */
    [[nodiscard]] const std::vector<uint8_t>& get_mask() const noexcept { 
        return mask_; 
    }

    /**
     * @brief Check if pattern is valid
     * @return true if pattern is not empty
     */
    [[nodiscard]] bool is_valid() const noexcept { 
        return length_ > 0; 
    }

    /**
     * @brief Get pattern as string (for logging/debugging)
     * @return Human-readable pattern string
     * 
     * Example: "48 8B 0D ?? ?? ?? ??"
     */
    [[nodiscard]] std::string to_string() const;

private:
    std::vector<uint8_t> bytes_;  ///< Pattern bytes (wildcards = 0x00)
    std::vector<uint8_t> mask_;   ///< Mask: 0xFF = exact match, 0x00 = wildcard
    size_t length_;               ///< Pattern length in bytes

    // Private constructor - use compile() factory method
    CompiledPattern() : length_(0) {}

    /**
     * @brief Validate and parse a single hex token
     * @param token Token string (e.g., "48" or "?")
     * @param out_byte Output byte value
     * @param out_mask Output mask value
     * @return true if token is valid
     */
    static bool parse_token(const std::string& token, uint8_t& out_byte, uint8_t& out_mask);

    /**
     * @brief Check if character is valid hex digit
     */
    static bool is_hex_digit(char c) noexcept;

    /**
     * @brief Convert hex character to value (0-15)
     */
    static uint8_t hex_char_to_value(char c) noexcept;
};

} // namespace ArgoSentry

#endif // ARGOSENTRY_COMPILED_PATTERN_HH
