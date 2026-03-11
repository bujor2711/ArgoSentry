#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

using DWORD = unsigned long;

namespace VolkDMA {
namespace MemoryLayout {

/**
 * @brief Memory region information
 * 
 * Represents a single memory region in a process with its properties
 */
struct MemoryRegion {
    uint64_t base_address;      // Start address of region
    uint64_t size;              // Size in bytes
    uint32_t protection;        // PAGE_EXECUTE_READ, PAGE_READWRITE, etc.
    uint32_t type;              // MEM_IMAGE, MEM_MAPPED, MEM_PRIVATE
    uint32_t state;             // MEM_COMMIT, MEM_FREE, MEM_RESERVE
    std::string module_name;    // Module name if MEM_IMAGE (e.g., "ntdll.dll", "game.exe")
    
    /**
     * @brief Check if region is executable
     */
    [[nodiscard]] bool is_executable() const;
    
    /**
     * @brief Check if region is writable
     */
    [[nodiscard]] bool is_writable() const;
    
    /**
     * @brief Check if region is readable
     */
    [[nodiscard]] bool is_readable() const;
    
    /**
     * @brief Check if region is committed (actually allocated)
     */
    [[nodiscard]] bool is_committed() const;
    
    /**
     * @brief Check if region is an image (loaded module)
     */
    [[nodiscard]] bool is_image() const;
    
    /**
     * @brief Get end address of region
     */
    [[nodiscard]] uint64_t end_address() const;
    
    /**
     * @brief Check if address is within this region
     */
    [[nodiscard]] bool contains(uint64_t address) const;
};

/**
 * @brief Memory layout analyzer for smart scanning
 * 
 * Analyzes process memory layout to enable intelligent signature scanning
 * that only searches relevant regions (e.g., executable code sections)
 */
class MemoryLayoutAnalyzer {
public:
    /**
     * @brief Get complete memory layout of a process
     * @param process_id Target process ID
     * @return Vector of all memory regions
     */
    [[nodiscard]] std::vector<MemoryRegion> get_memory_layout(DWORD process_id) const;
    
    /**
     * @brief Get only executable memory regions
     * @param process_id Target process ID
     * @return Vector of executable regions (.text sections, etc.)
     */
    [[nodiscard]] std::vector<MemoryRegion> get_executable_regions(DWORD process_id) const;
    
    /**
     * @brief Get only readable memory regions
     * @param process_id Target process ID
     * @return Vector of readable regions
     */
    [[nodiscard]] std::vector<MemoryRegion> get_readable_regions(DWORD process_id) const;
    
    /**
     * @brief Get memory regions for a specific module
     * @param process_id Target process ID
     * @param module_name Module name (e.g., "ntdll.dll", "game.exe")
     * @return Vector of regions belonging to that module
     */
    [[nodiscard]] std::vector<MemoryRegion> get_module_regions(
        DWORD process_id, 
        const std::string& module_name) const;
    
    /**
     * @brief Find base address of a specific module
     * @param process_id Target process ID
     * @param module_name Module name (case-insensitive)
     * @return Optional containing region if found
     */
    [[nodiscard]] std::optional<MemoryRegion> find_module(
        DWORD process_id,
        const std::string& module_name) const;
    
    /**
     * @brief Get total memory size of a process (committed regions only)
     * @param process_id Target process ID
     * @return Total size in bytes
     */
    [[nodiscard]] uint64_t get_total_memory_size(DWORD process_id) const;
    
    /**
     * @brief Get memory region containing specific address
     * @param process_id Target process ID
     * @param address Address to find
     * @return Optional containing region if address is valid
     */
    [[nodiscard]] std::optional<MemoryRegion> get_region_at_address(
        DWORD process_id,
        uint64_t address) const;
    
    /**
     * @brief Print memory layout summary (for debugging)
     * @param process_id Target process ID
     */
    void print_memory_layout(DWORD process_id) const;
    
private:
    // Helper to convert module name to lowercase for case-insensitive comparison
    [[nodiscard]] static std::string to_lowercase(const std::string& str);
};

/**
 * @brief Memory protection flags (PAGE_* constants)
 * 
 * Using enum class for C++17 compatibility and to avoid Windows.h macro conflicts
 */
enum class Protection : uint32_t {
    NoAccess          = 0x01,
    ReadOnly          = 0x02,
    ReadWrite         = 0x04,
    WriteCopy         = 0x08,
    Execute           = 0x10,
    ExecuteRead       = 0x20,
    ExecuteReadWrite  = 0x40,
    ExecuteWriteCopy  = 0x80,
    Guard             = 0x100,
    NoCache           = 0x200,
    WriteCombine      = 0x400
};

/**
 * @brief Memory type flags (MEM_* type constants)
 */
enum class MemoryType : uint32_t {
    Image   = 0x1000000,
    Mapped  = 0x40000,
    Private = 0x20000
};

/**
 * @brief Memory state flags (MEM_* state constants)
 */
enum class MemoryState : uint32_t {
    Commit  = 0x1000,
    Free    = 0x10000,
    Reserve = 0x2000
};

} // namespace MemoryLayout
} // namespace VolkDMA
