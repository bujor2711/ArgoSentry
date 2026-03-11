// VolkDMA - Memory Dump Utilities
// v1.9 - Export and visualize process memory in multiple formats
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>

#ifdef _WIN32
using DWORD = unsigned long;
#endif

namespace VolkDMA {

// Forward declaration
class DMA;

//==============================================================================
// Dump Format Types
//==============================================================================

enum class DumpFormat {
    Binary,   // Raw binary dump (most compact)
    HexDump,  // Human-readable hex with ASCII (for analysis)
    CArray,   // C/C++ array format (for integration)
    IDA       // IDA Pro compatible format
};

//==============================================================================
// Dump Metadata
//==============================================================================

struct DumpMetadata {
    uint64_t base_address;
    uint64_t size;
    std::string process_name;
    DWORD process_id;
    std::string timestamp;
    DumpFormat format;
    std::string module_name;  // Optional: if dumping specific module
    
    // Constructor
    DumpMetadata();
};

//==============================================================================
// Memory Dumper Class
//==============================================================================

class MemoryDumper {
public:
    explicit MemoryDumper(DMA& dma);
    ~MemoryDumper() = default;
    
    // Disable copy, allow move
    MemoryDumper(const MemoryDumper&) = delete;
    MemoryDumper& operator=(const MemoryDumper&) = delete;
    MemoryDumper(MemoryDumper&&) = default;
    MemoryDumper& operator=(MemoryDumper&&) = default;
    
    //==========================================================================
    // Core Dump Methods
    //==========================================================================
    
    // Dump specific memory region
    bool dump_region(
        uint64_t start_address,
        uint64_t end_address,
        const std::string& filename,
        DWORD process_id,
        DumpFormat format = DumpFormat::Binary
    );
    
    // Dump entire module (uses memory layout analyzer)
    bool dump_module(
        const std::string& module_name,
        const std::string& filename,
        DWORD process_id,
        DumpFormat format = DumpFormat::Binary
    );
    
    // Create complete process snapshot (all committed regions)
    bool create_snapshot(
        DWORD process_id,
        const std::string& snapshot_directory
    );
    
    // Quick hex dump to console (for debugging)
    void print_hex_dump(
        uint64_t address,
        size_t size,
        DWORD process_id,
        size_t bytes_per_line = 16
    );
    
    //==========================================================================
    // Dump Comparison
    //==========================================================================
    
    // Compare two binary dumps and return changed addresses
    std::vector<uint64_t> compare_dumps(
        const std::string& dump1_path,
        const std::string& dump2_path
    );
    
    // Get detailed diff information
    struct MemoryDiff {
        uint64_t address;
        std::vector<uint8_t> before;
        std::vector<uint8_t> after;
    };
    
    std::vector<MemoryDiff> get_detailed_diff(
        const std::string& dump1_path,
        const std::string& dump2_path,
        size_t context_bytes = 0  // How many bytes around change to include
    );
    
    //==========================================================================
    // Metadata Operations
    //==========================================================================
    
    // Save metadata alongside dump
    bool save_metadata(
        const std::string& dump_path,
        const DumpMetadata& metadata
    );
    
    // Load metadata from .meta file
    std::optional<DumpMetadata> load_metadata(
        const std::string& dump_path
    );
    
    //==========================================================================
    // Configuration
    //==========================================================================
    
    // Set chunk size for reading (default: 1MB)
    void set_chunk_size(size_t size) { chunk_size_ = size; }
    size_t get_chunk_size() const { return chunk_size_; }
    
    // Enable/disable compression (future: zlib support)
    void set_compression(bool enable) { compression_enabled_ = enable; }
    bool is_compression_enabled() const { return compression_enabled_; }
    
    // Set whether to save metadata automatically
    void set_auto_metadata(bool enable) { auto_metadata_ = enable; }
    bool is_auto_metadata_enabled() const { return auto_metadata_; }
    
private:
    //==========================================================================
    // Internal Helpers
    //==========================================================================
    
    // Format converters
    std::string format_hex_line(
        uint64_t address,
        const std::vector<uint8_t>& data,
        size_t offset,
        size_t bytes_per_line
    );
    
    std::string format_carray(
        const std::vector<uint8_t>& data,
        uint64_t base_address,
        const std::string& array_name = "memory_dump"
    );
    
    bool write_ida_script(
        const std::string& binary_path,
        uint64_t base_address,
        size_t size
    );
    
    // File I/O helpers
    bool write_binary_dump(
        const std::string& filename,
        const std::vector<uint8_t>& data
    );
    
    bool write_hex_dump(
        const std::string& filename,
        uint64_t base_address,
        const std::vector<uint8_t>& data
    );
    
    bool write_carray_dump(
        const std::string& filename,
        uint64_t base_address,
        const std::vector<uint8_t>& data
    );
    
    // Metadata helpers
    std::string generate_timestamp() const;
    std::string format_to_string(DumpFormat format) const;
    DumpFormat string_to_format(const std::string& str) const;
    
    //==========================================================================
    // Member Variables
    //==========================================================================
    
    DMA& dma_;                      // Reference to DMA instance
    size_t chunk_size_;             // Read chunk size (default: 1MB)
    bool compression_enabled_;      // Compression support (future)
    bool auto_metadata_;            // Auto-save metadata
};

} // namespace VolkDMA
