// ArgoSentry Offset Finder v3.1
// AOB signature to offset conversion for portable addresses
// Part of Phase 3.1 FAZA 2: Advanced RE Tools

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <cstdint>
#include <Windows.h>

namespace ArgoSentry {

// Forward declarations
class DMA;
class ModuleEnumerator;
class EnhancedPatternScanner;

/**
 * @brief Offset information for portable addressing
 * @since v3.1 FAZA 2
 */
struct OffsetInfo {
    std::string name;                  ///< Offset name (e.g., "player_base")
    std::string module_name;           ///< Target module (e.g., "game.exe")
    uint64_t rva;                      ///< Relative virtual address (offset from module base)
    std::string signature;             ///< AOB signature used to find this offset
    uint64_t last_absolute_address;    ///< Last resolved absolute address
    bool is_valid;                     ///< Validation status
    
    /**
     * @brief Get absolute address for current module base
     * @param module_base Current module base address
     * @return Absolute address
     */
    [[nodiscard]] uint64_t get_absolute(uint64_t module_base) const noexcept {
        return module_base + rva;
    }
};

/**
 * @brief Offset finder for portable address management
 * @details Converts AOB signatures to RVA offsets for version independence
 * @since v3.1 FAZA 2
 * 
 * Example:
 *   OffsetFinder finder(dma, scanner, enumerator, pid);
 *   
 *   // Find offset from signature
 *   auto offset = finder.find_offset(
 *       "player_base",
 *       "game.exe",
 *       "48 8B 05 ?? ?? ?? ??"
 *   );
 *   
 *   // Use offset (works across game updates)
 *   uint64_t addr = offset->get_absolute(game_base);
 */
class OffsetFinder {
public:
    /**
     * @brief Construct offset finder
     * @param dma DMA instance
     * @param scanner Pattern scanner for signature matching
     * @param enumerator Module enumerator for address calculations
     * @param process_id Target process ID
     */
    explicit OffsetFinder(
        DMA* dma,
        EnhancedPatternScanner* scanner,
        ModuleEnumerator* enumerator,
        DWORD process_id
    );
    
    /**
     * @brief Find and register offset from signature
     * @param name Offset name
     * @param module_name Target module
     * @param signature AOB pattern (IDA-style)
     * @param search_start Optional search start (default: module base)
     * @param search_size Optional search size (default: module size)
     * @return Offset info if found
     * 
     * Usage:
     *   auto offset = finder.find_offset(
     *       "player_base",
     *       "game.exe",
     *       "48 8B 05 ?? ?? ?? ??"
     *   );
     *   
     *   if (offset) {
     *       std::cout << "RVA: 0x" << std::hex << offset->rva << "\n";
     *   }
     */
    [[nodiscard]] std::optional<OffsetInfo> find_offset(
        const std::string& name,
        const std::string& module_name,
        const std::string& signature,
        std::optional<uint64_t> search_start = std::nullopt,
        std::optional<uint32_t> search_size = std::nullopt
    );
    
    /**
     * @brief Register existing offset manually
     * @param name Offset name
     * @param module_name Target module
     * @param rva Relative virtual address
     * @param signature Optional signature for re-scanning
     * @return true if registered
     */
    bool register_offset(
        const std::string& name,
        const std::string& module_name,
        uint64_t rva,
        const std::string& signature = ""
    );
    
    /**
     * @brief Get registered offset
     * @param name Offset name
     * @return Offset info if found
     */
    [[nodiscard]] std::optional<OffsetInfo> get_offset(const std::string& name) const;
    
    /**
     * @brief Get absolute address from offset
     * @param name Offset name
     * @return Absolute address if offset is registered
     * 
     * Usage:
     *   auto addr = finder.get_absolute_address("player_base");
     *   if (addr) {
     *       auto player = dma->read<Player>(*addr, pid);
     *   }
     */
    [[nodiscard]] std::optional<uint64_t> get_absolute_address(const std::string& name);
    
    /**
     * @brief Update all offsets (re-scan if signatures available)
     * @return Number of successfully updated offsets
     * 
     * Use when game updates or module base changes
     */
    size_t update_all_offsets();
    
    /**
     * @brief Validate offset (check if address is still valid)
     * @param name Offset name
     * @return true if offset is valid
     */
    [[nodiscard]] bool validate_offset(const std::string& name);
    
    /**
     * @brief Validate all registered offsets
     * @return Map of name -> validation result
     */
    [[nodiscard]] std::map<std::string, bool> validate_all_offsets();
    
    /**
     * @brief Remove offset
     * @param name Offset name
     * @return true if removed
     */
    bool remove_offset(const std::string& name);
    
    /**
     * @brief Clear all offsets
     */
    void clear() noexcept;
    
    /**
     * @brief Get number of registered offsets
     */
    [[nodiscard]] size_t size() const noexcept { return offsets_.size(); }
    
    /**
     * @brief Get all offset names
     */
    [[nodiscard]] std::vector<std::string> get_offset_names() const;
    
    /**
     * @brief Export offsets to JSON file
     * @param filepath Output file path
     * @return true if successful
     * 
     * Saves offsets for later use (version-independent)
     */
    bool export_to_file(const std::string& filepath) const;
    
    /**
     * @brief Import offsets from JSON file
     * @param filepath Input file path
     * @return Number of imported offsets
     * 
     * Loads previously saved offsets
     */
    size_t import_from_file(const std::string& filepath);
    
    /**
     * @brief Convert signature to offset (one-time)
     * @param module_name Target module
     * @param signature AOB pattern
     * @return RVA if found
     * 
     * Quick conversion without registration
     */
    [[nodiscard]] std::optional<uint64_t> signature_to_offset(
        const std::string& module_name,
        const std::string& signature
    );

private:
    DMA* dma_;                                  ///< DMA instance
    EnhancedPatternScanner* scanner_;           ///< Pattern scanner
    ModuleEnumerator* enumerator_;              ///< Module enumerator
    DWORD process_id_;                          ///< Target process
    std::map<std::string, OffsetInfo> offsets_; ///< Registered offsets
    
    // Helper methods
    [[nodiscard]] std::optional<uint64_t> find_signature_in_module(
        const std::string& module_name,
        const std::string& signature,
        std::optional<uint64_t> search_start,
        std::optional<uint32_t> search_size
    );
};

/**
 * @brief Offset database manager
 * @details Manages offset collections for multiple games/processes
 * @since v3.1 FAZA 2
 */
class OffsetDatabase {
public:
    /**
     * @brief Add offset collection for a game
     * @param game_name Game identifier (e.g., "csgo", "valorant")
     * @param offsets Vector of offset information
     */
    void add_game(const std::string& game_name, const std::vector<OffsetInfo>& offsets);
    
    /**
     * @brief Get offsets for a game
     * @param game_name Game identifier
     * @return Offsets if found
     */
    [[nodiscard]] std::optional<std::vector<OffsetInfo>> get_game_offsets(
        const std::string& game_name
    ) const;
    
    /**
     * @brief Remove game offsets
     * @param game_name Game identifier
     * @return true if removed
     */
    bool remove_game(const std::string& game_name);
    
    /**
     * @brief Clear all games
     */
    void clear() noexcept;
    
    /**
     * @brief Get number of games
     */
    [[nodiscard]] size_t size() const noexcept { return database_.size(); }
    
    /**
     * @brief Get all game names
     */
    [[nodiscard]] std::vector<std::string> get_game_names() const;
    
    /**
     * @brief Export database to file
     * @param filepath Output file path
     * @return true if successful
     */
    bool export_to_file(const std::string& filepath) const;
    
    /**
     * @brief Import database from file
     * @param filepath Input file path
     * @return Number of imported games
     */
    size_t import_from_file(const std::string& filepath);

private:
    std::map<std::string, std::vector<OffsetInfo>> database_;
};

} // namespace ArgoSentry
