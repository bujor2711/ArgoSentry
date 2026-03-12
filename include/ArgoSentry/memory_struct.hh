// ArgoSentry Memory Structure Templates v3.1
// Automatic C++ struct reading with padding/alignment
// Part of Phase 3.1: Reverse Engineering Tools

#pragma once

#include <type_traits>
#include <optional>
#include <cstring>
#include <vector>
#include <map>
#include <string>

namespace ArgoSentry {

// Forward declarations
class DMA;

/**
 * @brief Template-based memory structure reader
 * @details Automatically reads C++ structs from memory with proper alignment
 * @since v3.1
 * 
 * Features:
 * - Automatic size calculation (sizeof with padding)
 * - POD (Plain Old Data) type validation
 * - Nested struct support
 * - Array member support
 * - Custom alignment handling
 * 
 * Example:
 *   struct Player {
 *       char name[64];
 *       int32_t health;
 *       int32_t mana;
 *       float position[3];
 *   };
 * 
 *   auto player = dma->read_struct<Player>(player_addr, pid);
 *   std::cout << "Health: " << player.health << "\n";
 */

/**
 * @brief Read a POD structure from memory
 * @tparam T Structure type (must be POD/trivially copyable)
 * @param dma DMA instance for memory reading
 * @param address Memory address to read from
 * @param process_id Target process ID
 * @return Structure instance if successful
 * 
 * Requirements:
 * - T must be trivially copyable (std::is_trivially_copyable_v<T>)
 * - T should not contain pointers (they become invalid)
 * - T should be packed or have known padding
 * 
 * Usage:
 *   auto data = read_struct<MyStruct>(dma, 0x140001000, pid);
 *   if (data) {
 *       std::cout << "Value: " << data->some_field << "\n";
 *   }
 */
template<typename T>
[[nodiscard]] std::optional<T> read_struct(DMA& dma, uint64_t address, DWORD process_id) {
    static_assert(std::is_trivially_copyable_v<T>, 
                  "T must be trivially copyable (POD type)");
    
    // Allocate buffer for structure
    T result{};
    
    // Read raw bytes
    std::vector<uint8_t> buffer(sizeof(T));
    if (!dma.read_raw(address, buffer.data(), sizeof(T), process_id)) {
        return std::nullopt;
    }
    
    // Copy to structure
    std::memcpy(&result, buffer.data(), sizeof(T));
    return result;
}

/**
 * @brief Read an array of structures from memory
 * @tparam T Structure type
 * @param dma DMA instance
 * @param address Starting address
 * @param count Number of elements
 * @param process_id Target process
 * @return Vector of structures if successful
 */
template<typename T>
[[nodiscard]] std::optional<std::vector<T>> read_struct_array(
    DMA& dma, 
    uint64_t address, 
    size_t count,
    DWORD process_id
) {
    static_assert(std::is_trivially_copyable_v<T>, 
                  "T must be trivially copyable");
    
    if (count == 0) {
        return std::vector<T>{};
    }
    
    // Read entire array in one operation
    const size_t total_size = sizeof(T) * count;
    std::vector<uint8_t> buffer(total_size);
    
    if (!dma.read_raw(address, buffer.data(), total_size, process_id)) {
        return std::nullopt;
    }
    
    // Convert to vector of structures
    std::vector<T> result(count);
    std::memcpy(result.data(), buffer.data(), total_size);
    return result;
}

/**
 * @brief Write a POD structure to memory
 * @tparam T Structure type
 * @param dma DMA instance
 * @param address Memory address to write to
 * @param value Structure to write
 * @param process_id Target process
 * @return true if successful
 */
template<typename T>
bool write_struct(DMA& dma, uint64_t address, const T& value, DWORD process_id) {
    static_assert(std::is_trivially_copyable_v<T>, 
                  "T must be trivially copyable");
    
    // Write raw bytes
    return dma.write_raw(address, 
                         reinterpret_cast<const uint8_t*>(&value),
                         sizeof(T), 
                         process_id);
}

/**
 * @brief Write an array of structures to memory
 * @tparam T Structure type
 * @param dma DMA instance
 * @param address Starting address
 * @param values Vector of structures
 * @param process_id Target process
 * @return true if successful
 */
template<typename T>
bool write_struct_array(
    DMA& dma,
    uint64_t address,
    const std::vector<T>& values,
    DWORD process_id
) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    
    if (values.empty()) {
        return true;
    }
    
    const size_t total_size = sizeof(T) * values.size();
    return dma.write_raw(address,
                         reinterpret_cast<const uint8_t*>(values.data()),
                         total_size,
                         process_id);
}

/**
 * @brief Validate struct size matches expected size
 * @tparam T Structure type
 * @param expected_size Expected size in bytes
 * @return true if sizes match
 * 
 * Useful for detecting padding issues:
 *   if (!validate_struct_size<Player>(0x50)) {
 *       std::cerr << "Warning: struct size mismatch!\n";
 *   }
 */
template<typename T>
[[nodiscard]] constexpr bool validate_struct_size(size_t expected_size) noexcept {
    return sizeof(T) == expected_size;
}

/**
 * @brief Get struct size with alignment information
 * @tparam T Structure type
 * @return Pair of (size, alignment)
 */
template<typename T>
[[nodiscard]] constexpr std::pair<size_t, size_t> get_struct_info() noexcept {
    return {sizeof(T), alignof(T)};
}

/**
 * @brief Nested structure reader with offset
 * @details Read a nested struct at base_address + offset
 * @tparam T Structure type
 * @param dma DMA instance
 * @param base_address Base address
 * @param offset Offset from base
 * @param process_id Target process
 * @return Structure if successful
 */
template<typename T>
[[nodiscard]] std::optional<T> read_nested_struct(
    DMA& dma,
    uint64_t base_address,
    uint64_t offset,
    DWORD process_id
) {
    return read_struct<T>(dma, base_address + offset, process_id);
}

/**
 * @brief Memory structure manager for common game structures
 * @details Provides named access to frequently used struct types
 * @since v3.1
 */
class MemoryStructManager {
public:
    /**
     * @brief Register a named structure offset
     * @param name Structure identifier (e.g., "player", "entity")
     * @param base_address Base address (usually module base)
     * @param offset Offset from base
     */
    void register_struct(const std::string& name, uint64_t base_address, uint64_t offset);
    
    /**
     * @brief Get registered structure address
     * @param name Structure identifier
     * @return Address if registered
     */
    [[nodiscard]] std::optional<uint64_t> get_address(const std::string& name) const;
    
    /**
     * @brief Read registered structure
     * @tparam T Structure type
     * @param name Structure identifier
     * @param dma DMA instance
     * @param process_id Target process
     * @return Structure if successful
     */
    template<typename T>
    [[nodiscard]] std::optional<T> read(
        const std::string& name,
        DMA& dma,
        DWORD process_id
    ) const {
        auto addr = get_address(name);
        if (!addr) {
            return std::nullopt;
        }
        return read_struct<T>(dma, *addr, process_id);
    }
    
    /**
     * @brief Update base address for all registered structs
     * @param old_base Old base address
     * @param new_base New base address
     */
    void update_base(uint64_t old_base, uint64_t new_base);
    
    /**
     * @brief Remove registered structure
     */
    bool unregister_struct(const std::string& name);
    
    /**
     * @brief Clear all registered structures
     */
    void clear() noexcept;
    
    /**
     * @brief Get all registered names
     */
    [[nodiscard]] std::vector<std::string> get_names() const;

private:
    struct StructInfo {
        uint64_t base_address;
        uint64_t offset;
    };
    
    std::map<std::string, StructInfo> structs_;
};

} // namespace ArgoSentry
