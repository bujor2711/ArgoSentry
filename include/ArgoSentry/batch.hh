#ifndef VOLKDMA_BATCH_HH
#define VOLKDMA_BATCH_HH

#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include <chrono>
#include <Windows.h>

namespace ArgoSentry {

/// Request for a single read operation in a batch
struct ReadRequest {
    uint64_t address;              ///< Memory address to read from
    size_t size;                   ///< Number of bytes to read
    void* destination;             ///< Destination buffer (optional, can be nullptr)
    
    // Output fields (filled after execution)
    bool success;                  ///< Whether the read succeeded
    size_t bytes_read;             ///< Number of bytes actually read
    std::string error_message;     ///< Error message if failed
    
    /// Constructor with address and size
    ReadRequest(uint64_t addr, size_t sz)
        : address(addr), size(sz), destination(nullptr),
          success(false), bytes_read(0) {}
    
    /// Constructor with address, size, and destination buffer
    ReadRequest(uint64_t addr, size_t sz, void* dest)
        : address(addr), size(sz), destination(dest),
          success(false), bytes_read(0) {}
};

/// Result of a batch read operation
struct BatchReadResult {
    size_t successful_reads;       ///< Number of successful reads
    size_t failed_reads;           ///< Number of failed reads
    size_t total_bytes_read;       ///< Total bytes successfully read
    std::chrono::microseconds duration;  ///< Time taken for operation
    double throughput_mbps;        ///< Throughput in MB/s
    std::vector<size_t> failed_indices;  ///< Indices of failed requests
    
    /// Constructor
    BatchReadResult()
        : successful_reads(0), failed_reads(0), total_bytes_read(0),
          duration(0), throughput_mbps(0.0) {}
    
    /// Get success rate (0.0 to 1.0)
    double get_success_rate() const {
        size_t total = successful_reads + failed_reads;
        return total > 0 ? static_cast<double>(successful_reads) / total : 0.0;
    }
    
    /// Check if all reads succeeded
    bool all_succeeded() const {
        return failed_reads == 0 && successful_reads > 0;
    }
    
    /// Check if any reads succeeded
    bool any_succeeded() const {
        return successful_reads > 0;
    }
};

/// Statistics for batch operations
struct BatchStatistics {
    size_t total_batch_operations;      ///< Total batch operations performed
    size_t total_individual_reads;      ///< Total individual reads in batches
    size_t total_successful_reads;      ///< Total successful reads
    size_t total_failed_reads;          ///< Total failed reads
    uint64_t total_bytes_read;          ///< Total bytes read
    std::chrono::microseconds total_duration;  ///< Total time spent
    
    // Optimization statistics
    size_t cache_hits_avoided;          ///< Reads served from cache
    size_t pages_grouped;               ///< Number of page groupings performed
    size_t addresses_sorted;            ///< Number of times addresses were sorted
    
    /// Constructor
    BatchStatistics()
        : total_batch_operations(0), total_individual_reads(0),
          total_successful_reads(0), total_failed_reads(0),
          total_bytes_read(0), total_duration(0),
          cache_hits_avoided(0), pages_grouped(0), addresses_sorted(0) {}
    
    /// Get average reads per batch
    double get_average_reads_per_batch() const {
        return total_batch_operations > 0 
            ? static_cast<double>(total_individual_reads) / total_batch_operations 
            : 0.0;
    }
    
    /// Get overall success rate
    double get_success_rate() const {
        size_t total = total_successful_reads + total_failed_reads;
        return total > 0 
            ? static_cast<double>(total_successful_reads) / total 
            : 0.0;
    }
    
    /// Get average throughput in MB/s
    double get_average_throughput_mbps() const {
        if (total_duration.count() == 0) return 0.0;
        double seconds = total_duration.count() / 1000000.0;
        double mb = total_bytes_read / (1024.0 * 1024.0);
        return mb / seconds;
    }
};

/// Batch operations manager
class BatchOperations {
public:
    /// Constructor
    BatchOperations();
    
    /// Destructor
    ~BatchOperations();
    
    // Prevent copying
    BatchOperations(const BatchOperations&) = delete;
    BatchOperations& operator=(const BatchOperations&) = delete;
    
    /// Execute batch read operation
    /// @param requests Vector of read requests (modified in-place with results)
    /// @param process_id Target process ID
    /// @param optimize Enable optimizations (sorting, grouping, caching)
    /// @return Batch operation result
    BatchReadResult batch_read(
        std::vector<ReadRequest>& requests,
        DWORD process_id,
        bool optimize = true
    );
    
    /// Type-safe batch read for same-type values
    /// @tparam T Type of values to read
    /// @param addresses Vector of addresses to read from
    /// @param process_id Target process ID
    /// @return Vector of optional values (nullopt if read failed)
    template<typename T>
    std::vector<std::optional<T>> batch_read_typed(
        const std::vector<uint64_t>& addresses,
        DWORD process_id
    );
    
    /// Read contiguous memory range in chunks
    /// @param start_address Start of memory range
    /// @param end_address End of memory range
    /// @param chunk_size Size of each chunk (default: 4096 bytes)
    /// @param process_id Target process ID
    /// @return Vector containing all read data
    std::vector<uint8_t> batch_read_range(
        uint64_t start_address,
        uint64_t end_address,
        size_t chunk_size,
        DWORD process_id
    );
    
    /// Get batch operation statistics
    BatchStatistics get_statistics() const;
    
    /// Reset statistics
    void reset_statistics();
    
    /// Enable/disable statistics collection
    void set_statistics_enabled(bool enabled);
    
    /// Check if statistics are enabled
    bool is_statistics_enabled() const;

    /// Set VMM handle for batch operations (internal use)
    void set_vmm_handle(void* handle);

private:
    struct Impl;
    Impl* pimpl_;
    
    // Optimization helpers
    void optimize_requests(std::vector<ReadRequest>& requests);
    void sort_by_address(std::vector<ReadRequest>& requests);
    void group_by_page(std::vector<ReadRequest>& requests);
};

/// Template implementation
template<typename T>
std::vector<std::optional<T>> BatchOperations::batch_read_typed(
    const std::vector<uint64_t>& addresses,
    DWORD process_id
) {
    static_assert(std::is_trivially_copyable<T>::value, 
                  "T must be trivially copyable");
    static_assert(!std::is_pointer<T>::value, 
                  "T cannot be a pointer type");
    static_assert(sizeof(T) <= 256, 
                  "T is too large (max 256 bytes)");
    
    std::vector<std::optional<T>> results;
    results.reserve(addresses.size());
    
    // Create read requests
    std::vector<ReadRequest> requests;
    requests.reserve(addresses.size());
    
    std::vector<T> buffers(addresses.size());
    
    for (size_t i = 0; i < addresses.size(); ++i) {
        requests.emplace_back(addresses[i], sizeof(T), &buffers[i]);
    }
    
    // Execute batch read
    auto result = batch_read(requests, process_id, true);
    
    // Convert to optional results
    for (size_t i = 0; i < requests.size(); ++i) {
        if (requests[i].success) {
            results.push_back(buffers[i]);
        } else {
            results.push_back(std::nullopt);
        }
    }
    
    return results;
}

} // namespace ArgoSentry

#endif // VOLKDMA_BATCH_HH

