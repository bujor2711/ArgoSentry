#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <functional>
#include <mutex>
#include <chrono>
#include <memory>
#include <Windows.h>

namespace ArgoSentry {

/**
 * @brief Abstract DMA interface for dependency injection
 * 
 * Allows testing without hardware by providing a mock implementation.
 * Real DMA class can inherit from this interface for production use.
 */
class IDMAInterface {
public:
    virtual ~IDMAInterface() = default;

    // Basic read operations
    virtual uint8_t read_u8(uint64_t address, DWORD pid) = 0;
    virtual uint16_t read_u16(uint64_t address, DWORD pid) = 0;
    virtual uint32_t read_u32(uint64_t address, DWORD pid) = 0;
    virtual uint64_t read_u64(uint64_t address, DWORD pid) = 0;

    // Bulk read
    virtual std::vector<uint8_t> read_bytes(uint64_t address, size_t size, DWORD pid) = 0;

    // Signature scanning
    virtual uint64_t find_signature(const char* pattern,
                                   uint64_t start, uint64_t end,
                                   DWORD pid) = 0;

    // Process management
    virtual DWORD get_process_id(const char* name) = 0;
    virtual std::vector<DWORD> get_process_id_list(const char* name) = 0;
};

/**
 * @brief Mock DMA implementation for testing without hardware
 * 
 * Provides in-memory simulation of DMA operations with:
 * - Memory regions with validation
 * - Process simulation
 * - Statistics tracking
 * - Memory limits (prevents OOM)
 * - LRU eviction strategy
 * - Thread-safe operations
 * 
 * @note Not a replacement for hardware testing, but useful for:
 *       - Unit tests
 *       - CI/CD pipelines
 *       - Algorithm development
 *       - Reproducible test scenarios
 */
class MockDMA : public IDMAInterface {
public:
    // Memory and address limits
    static constexpr size_t MAX_MEMORY_SIZE = 100 * 1024 * 1024;  // 100MB limit
    static constexpr uint64_t MIN_VALID_ADDRESS = 0x10000;        // NULL guard
    static constexpr uint64_t MAX_VALID_ADDRESS = 0x7FFFFFFFFFFF; // 48-bit address space

    /**
     * @brief Statistics for mock DMA operations
     */
    struct Statistics {
        size_t read_count{0};
        size_t find_count{0};
        size_t cache_hits{0};
        size_t cache_misses{0};
        size_t evictions{0};

        void reset() {
            read_count = 0;
            find_count = 0;
            cache_hits = 0;
            cache_misses = 0;
            evictions = 0;
        }
    };

private:
    struct MemoryRegion {
        std::vector<uint8_t> data;
        uint64_t base_address;
        std::chrono::steady_clock::time_point last_access;
    };

    std::unordered_map<uint64_t, MemoryRegion> memory_regions_;
    std::unordered_map<std::string, DWORD> mock_processes_;
    size_t total_memory_used_{0};
    mutable std::mutex mutex_;  // Thread safety
    Statistics stats_;

    // Helper: Validate address range
    bool validate_address(uint64_t addr) const;

    // Helper: Find memory region containing address
    std::optional<std::reference_wrapper<MemoryRegion>> find_region(uint64_t address);

    // Helper: Parse signature pattern
    struct PatternByte {
        uint8_t value;
        bool is_wildcard;
    };
    std::vector<PatternByte> parse_pattern(const char* pattern) const;

    // Helper: Match pattern in buffer
    bool match_pattern(const uint8_t* data, size_t size,
                      const std::vector<PatternByte>& pattern) const;

public:
    MockDMA() = default;
    ~MockDMA() override = default;

    // Delete copy/move (not needed for mock)
    MockDMA(const MockDMA&) = delete;
    MockDMA& operator=(const MockDMA&) = delete;
    MockDMA(MockDMA&&) = delete;
    MockDMA& operator=(MockDMA&&) = delete;

    /**
     * @brief Set memory region with validation
     * @param addr Base address of the region
     * @param data Memory data to store
     * @throws std::invalid_argument if address invalid
     * @throws std::runtime_error if memory limit exceeded
     */
    void set_memory(uint64_t addr, const std::vector<uint8_t>& data);

    /**
     * @brief Register a mock process
     * @param name Process name (e.g., "test.exe")
     * @param pid Process ID to associate
     */
    void set_process(const std::string& name, DWORD pid);

    /**
     * @brief Clear all mock data and statistics
     */
    void clear();

    /**
     * @brief Evict oldest accessed memory region (LRU)
     * Used when approaching memory limits
     */
    void evict_oldest_region();

    /**
     * @brief Get current statistics
     */
    [[nodiscard]] const Statistics& get_statistics() const;

    /**
     * @brief Get total memory usage in bytes
     */
    [[nodiscard]] size_t get_memory_usage() const;

    /**
     * @brief Get number of memory regions
     */
    [[nodiscard]] size_t get_region_count() const;

    // IDMAInterface implementation
    uint8_t read_u8(uint64_t address, DWORD pid) override;
    uint16_t read_u16(uint64_t address, DWORD pid) override;
    uint32_t read_u32(uint64_t address, DWORD pid) override;
    uint64_t read_u64(uint64_t address, DWORD pid) override;

    std::vector<uint8_t> read_bytes(uint64_t address, size_t size, DWORD pid) override;

    uint64_t find_signature(const char* pattern, uint64_t start,
                           uint64_t end, DWORD pid) override;

    DWORD get_process_id(const char* name) override;
    std::vector<DWORD> get_process_id_list(const char* name) override;
};

} // namespace ArgoSentry
