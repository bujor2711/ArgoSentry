#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <optional>
#include <cstring>

// DWORD type definition
using DWORD = unsigned long;

namespace ArgoSentry {

class DMA; // Forward declaration

// Represents a memory difference between two snapshots
struct MemoryDiff {
    uint64_t address;           // Address where change occurred
    std::vector<uint8_t> before; // Bytes before change
    std::vector<uint8_t> after;  // Bytes after change
    size_t size;                 // Number of bytes changed

    // Constructor
    MemoryDiff(uint64_t addr, const std::vector<uint8_t>& b, 
               const std::vector<uint8_t>& a)
        : address(addr), before(b), after(a), size(b.size()) {}

    MemoryDiff() : address(0), size(0) {}
};

// Configuration for memory diffing operations
struct DiffConfig {
    size_t min_change_size = 1;      // Minimum size of change to report (bytes)
    size_t max_change_size = 256;    // Maximum size of change to report (bytes)
    bool group_adjacent = true;      // Group adjacent changed bytes
    size_t adjacency_threshold = 16; // Max gap between adjacent changes (bytes)
    size_t max_results = 10000;      // Maximum number of diffs to return
    
    DiffConfig() = default;
};

// Statistics for diff operations
struct DiffStatistics {
    size_t total_bytes_compared = 0;
    size_t total_changes_found = 0;
    size_t bytes_changed = 0;
    std::chrono::milliseconds duration{0};
    double change_percentage = 0.0;

    void calculate_percentage() {
        if (total_bytes_compared > 0) {
            change_percentage = (static_cast<double>(bytes_changed) / 
                               static_cast<double>(total_bytes_compared)) * 100.0;
        }
    }
};

// Memory Differ - detects changes in memory over time
class MemoryDiffer {
public:
    explicit MemoryDiffer(const DiffConfig& config = DiffConfig());
    ~MemoryDiffer() = default;

    // Compare two memory dump files
    std::vector<MemoryDiff> compare_snapshots(
        const std::string& snapshot1_path,
        const std::string& snapshot2_path);

    // Compare two memory buffers
    std::vector<MemoryDiff> compare_buffers(
        const uint8_t* buffer1, size_t size1,
        const uint8_t* buffer2, size_t size2,
        uint64_t base_address = 0);

    // Live diffing - find what changed between two reads
    std::vector<uint64_t> find_changed_addresses(
        DMA& dma,
        uint64_t start_address,
        uint64_t end_address,
        DWORD process_id,
        std::chrono::milliseconds interval);

    // Live diffing with detailed diff information
    std::vector<MemoryDiff> compare_regions(
        DMA& dma,
        uint64_t start_address,
        uint64_t end_address,
        DWORD process_id,
        std::chrono::milliseconds interval);

    // Find addresses that match a specific value
    std::vector<uint64_t> find_value(
        DMA& dma,
        uint64_t start_address,
        uint64_t end_address,
        DWORD process_id,
        const std::vector<uint8_t>& value);

    // Narrow down previous results to values that changed
    std::vector<uint64_t> filter_changed(
        DMA& dma,
        const std::vector<uint64_t>& addresses,
        DWORD process_id,
        std::chrono::milliseconds interval);

    // Find addresses matching a specific type value
    template<typename T>
    std::vector<uint64_t> find_value_typed(
        DMA& dma,
        uint64_t start_address,
        uint64_t end_address,
        DWORD process_id,
        T value);

    // Configuration
    void set_config(const DiffConfig& config) { config_ = config; }
    DiffConfig get_config() const { return config_; }

    // Statistics
    DiffStatistics get_statistics() const { return stats_; }
    void reset_statistics();

private:
    DiffConfig config_;
    DiffStatistics stats_;

    // Helper functions
    std::vector<MemoryDiff> diff_buffers_impl(
        const uint8_t* buf1, size_t size1,
        const uint8_t* buf2, size_t size2,
        uint64_t base_addr);

    bool should_group_with_previous(const MemoryDiff& prev, 
                                    uint64_t current_addr) const;
};

// Template implementation
template<typename T>
std::vector<uint64_t> MemoryDiffer::find_value_typed(
    DMA& dma,
    uint64_t start_address,
    uint64_t end_address,
    DWORD process_id,
    T value)
{
    std::vector<uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    return find_value(dma, start_address, end_address, process_id, bytes);
}

} // namespace ArgoSentry
