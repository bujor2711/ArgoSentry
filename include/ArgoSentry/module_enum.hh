// ArgoSentry Module Enumerator v3.1
// Automatic module enumeration and information retrieval
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

/**
 * @brief Information about a loaded module
 * @since v3.1 FAZA 2
 */
struct ModuleInfo {
    std::string name;              ///< Module name (e.g., "game.exe", "kernel32.dll")
    std::string full_path;         ///< Full path to module
    uint64_t base_address;         ///< Module base address in process memory
    uint32_t size;                 ///< Module size in bytes
    uint64_t entry_point;          ///< Entry point address (base + entry RVA)
    
    /**
     * @brief Check if this is the main executable
     * @return true if module is the main .exe
     */
    [[nodiscard]] bool is_main_module() const noexcept;
    
    /**
     * @brief Get module end address
     * @return base_address + size
     */
    [[nodiscard]] uint64_t end_address() const noexcept {
        return base_address + size;
    }
    
    /**
     * @brief Check if address is within this module
     * @param address Address to check
     * @return true if address is in [base, base+size)
     */
    [[nodiscard]] bool contains_address(uint64_t address) const noexcept {
        return address >= base_address && address < end_address();
    }
};

/**
 * @brief Module enumerator for process analysis
 * @details Lists all loaded modules (DLLs, EXE) in a process
 * @since v3.1 FAZA 2
 * 
 * Example:
 *   ModuleEnumerator enumerator(dma, pid);
 *   auto modules = enumerator.enumerate_modules();
 *   
 *   for (const auto& mod : modules) {
 *       std::cout << mod.name << " @ 0x" << std::hex << mod.base_address << "\n";
 *   }
 */
class ModuleEnumerator {
public:
    /**
     * @brief Construct module enumerator
     * @param dma DMA instance for memory operations
     * @param process_id Target process ID
     */
    explicit ModuleEnumerator(DMA* dma, DWORD process_id);
    
    /**
     * @brief Enumerate all loaded modules
     * @param force_refresh Force refresh (ignore cache)
     * @return Vector of module information
     * 
     * Usage:
     *   auto modules = enumerator.enumerate_modules();
     *   for (const auto& mod : modules) {
     *       std::cout << mod.name << " at 0x" << std::hex << mod.base_address << "\n";
     *   }
     */
    [[nodiscard]] std::vector<ModuleInfo> enumerate_modules(bool force_refresh = false);
    
    /**
     * @brief Find module by name
     * @param module_name Module name (case-insensitive, e.g., "kernel32.dll")
     * @return Module info if found
     * 
     * Usage:
     *   auto mod = enumerator.find_module("game.exe");
     *   if (mod) {
     *       uint64_t base = mod->base_address;
     *   }
     */
    [[nodiscard]] std::optional<ModuleInfo> find_module(const std::string& module_name);
    
    /**
     * @brief Find module by address
     * @param address Address to search for
     * @return Module containing the address
     * 
     * Usage:
     *   auto mod = enumerator.find_module_by_address(0x140001000);
     *   if (mod) {
     *       std::cout << "Address is in: " << mod->name << "\n";
     *   }
     */
    [[nodiscard]] std::optional<ModuleInfo> find_module_by_address(uint64_t address);
    
    /**
     * @brief Get main module (executable)
     * @return Main module info
     * 
     * Usage:
     *   auto main_mod = enumerator.get_main_module();
     *   if (main_mod) {
     *       uint64_t game_base = main_mod->base_address;
     *   }
     */
    [[nodiscard]] std::optional<ModuleInfo> get_main_module();
    
    /**
     * @brief Get module base address by name
     * @param module_name Module name
     * @return Base address if found
     */
    [[nodiscard]] std::optional<uint64_t> get_module_base(const std::string& module_name);
    
    /**
     * @brief Get module size by name
     * @param module_name Module name
     * @return Module size if found
     */
    [[nodiscard]] std::optional<uint32_t> get_module_size(const std::string& module_name);
    
    /**
     * @brief Calculate relative virtual address (RVA)
     * @param absolute_address Absolute address in process
     * @param module_name Module name (or nullopt for auto-detect)
     * @return RVA (offset from module base)
     * 
     * Usage:
     *   auto rva = enumerator.calculate_rva(0x140001234, "game.exe");
     *   // rva = 0x1234 if game.exe base is 0x140000000
     */
    [[nodiscard]] std::optional<uint64_t> calculate_rva(
        uint64_t absolute_address,
        const std::optional<std::string>& module_name = std::nullopt
    );
    
    /**
     * @brief Calculate absolute address from RVA
     * @param rva Relative virtual address (offset from module base)
     * @param module_name Module name
     * @return Absolute address
     * 
     * Usage:
     *   auto addr = enumerator.calculate_absolute(0x1234, "game.exe");
     *   // addr = 0x140001234 if game.exe base is 0x140000000
     */
    [[nodiscard]] std::optional<uint64_t> calculate_absolute(
        uint64_t rva,
        const std::string& module_name
    );
    
    /**
     * @brief Get number of loaded modules
     */
    [[nodiscard]] size_t module_count() const noexcept { return modules_.size(); }
    
    /**
     * @brief Clear module cache
     */
    void clear_cache() noexcept;
    
    /**
     * @brief Enable/disable caching
     * @param enable Enable caching
     */
    void enable_cache(bool enable) noexcept { cache_enabled_ = enable; }
    
    /**
     * @brief Check if cache is enabled
     */
    [[nodiscard]] bool is_cache_enabled() const noexcept { return cache_enabled_; }
    
    /**
     * @brief Export module list to file
     * @param filepath Output file path
     * @return true if successful
     */
    bool export_to_file(const std::string& filepath) const;

private:
    DMA* dma_;                                      ///< DMA instance
    DWORD process_id_;                              ///< Target process ID
    std::vector<ModuleInfo> modules_;               ///< Cached modules
    bool cache_enabled_{true};                      ///< Enable module caching
    
    // Helper methods
    [[nodiscard]] std::string normalize_module_name(const std::string& name) const;
    [[nodiscard]] bool refresh_modules();
};

/**
 * @brief Module manager for multiple processes
 * @details Manages module enumerators for multiple processes
 * @since v3.1 FAZA 2
 */
class ModuleManager {
public:
    /**
     * @brief Add process to management
     * @param process_id Process ID
     * @param enumerator Module enumerator
     */
    void add_process(DWORD process_id, ModuleEnumerator&& enumerator);
    
    /**
     * @brief Get enumerator for process
     * @param process_id Process ID
     * @return Enumerator if found
     */
    [[nodiscard]] ModuleEnumerator* get_enumerator(DWORD process_id) noexcept;
    
    /**
     * @brief Remove process
     * @param process_id Process ID
     * @return true if removed
     */
    bool remove_process(DWORD process_id);
    
    /**
     * @brief Clear all processes
     */
    void clear() noexcept;
    
    /**
     * @brief Get number of managed processes
     */
    [[nodiscard]] size_t size() const noexcept { return enumerators_.size(); }

private:
    std::map<DWORD, ModuleEnumerator> enumerators_;
};

} // namespace ArgoSentry
