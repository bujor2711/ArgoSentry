#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <optional>

#include "compiled_pattern.hh"  // v2.5 - Compiled pattern support

// Forward declarations
namespace ArgoSentry {
    // Batch operations
    struct ReadRequest;
    struct BatchReadResult;
    class BatchOperations;

    // Metrics
    namespace Metrics {
        class MetricsCollector;
    }

    // Cache
    namespace Cache {
        class MemoryCache;
    }

    // Health monitoring
    enum class HealthStatus;
    class HealthMonitor;

    // Memory layout
    namespace MemoryLayout {
        class MemoryLayoutAnalyzer;
    }

    // Memory diffing (v2.1)
    class MemoryDiffer;
    struct MemoryDiff;
    struct DiffConfig;

    // Rate limiting (v2.3)
    class RateLimiter;

    // Builder pattern (v2.2)
    class DMABuilder;
}

// DumpFormat enum (from dumper.hh)
namespace ArgoSentry {
    enum class DumpFormat {
        Binary,
        Hex,
        CArray
    };
}

struct tdVMM_HANDLE;
using VMM_HANDLE = tdVMM_HANDLE*;

extern "C" void VMMDLL_Close(VMM_HANDLE);
inline constexpr auto vmm_close = [](VMM_HANDLE h) noexcept { if (h) VMMDLL_Close(h); };
using ArgoHandle = std::unique_ptr<std::remove_pointer_t<VMM_HANDLE>, decltype(vmm_close)>;

using DWORD = unsigned long;

namespace ArgoSentry {

class DMA {
public:
    explicit DMA(bool use_memory_map = true);
    ~DMA();

    /**
     * @brief Create a fluent builder for DMA configuration
     * @return DMABuilder instance for method chaining
     * @since v2.2
     * 
     * Example:
     * @code
     * auto dma = DMA::Builder()
     *     .with_cache(100 * 1024 * 1024)
     *     .with_metrics(true)
     *     .build();
     * @endcode
     */
    static DMABuilder Builder();

    ArgoHandle handle;

    [[nodiscard]] DWORD get_process_id(const std::string& process_name) const;
    [[nodiscard]] std::vector<DWORD> get_process_id_list(const std::string& process_name) const;

    // Signature scanning - string patterns
    [[nodiscard]] uint64_t find_signature(const char* signature, uint64_t range_start, uint64_t range_end, DWORD process_id) const;

    // Signature scanning - compiled patterns (v2.5 - 2-3x faster for reused patterns)
    [[nodiscard]] uint64_t find_signature(const CompiledPattern& pattern, uint64_t range_start, uint64_t range_end, DWORD process_id) const;

    // Smart signature scanning using memory layout analysis
    [[nodiscard]] uint64_t find_signature_in_module(const char* signature, const std::string& module_name, DWORD process_id) const;
    [[nodiscard]] uint64_t find_signature_in_module(const CompiledPattern& pattern, const std::string& module_name, DWORD process_id) const;

    [[nodiscard]] uint64_t find_signature_in_executable(const char* signature, DWORD process_id) const;
    [[nodiscard]] uint64_t find_signature_in_executable(const CompiledPattern& pattern, DWORD process_id) const;

    template<typename T>
    [[nodiscard]] T read(uint64_t address, DWORD process_id) const;

    // Metrics access
    const Metrics::MetricsCollector& get_metrics() const;
    void reset_metrics();
    void log_metrics_summary() const;
    void log_metrics_detailed() const;

    // Cache management
    void enable_cache(bool enable);
    void clear_cache();
    void invalidate_cache(uint64_t address);
    size_t get_cache_size() const;
    size_t get_cache_entry_count() const;
    void log_cache_statistics() const;

    // Cache configuration (v2.2)
    void set_cache_size(size_t size);
    void set_cache_ttl(std::chrono::seconds ttl);
    size_t get_cache_max_size() const;
    std::chrono::seconds get_cache_ttl() const;

    // Metrics configuration (v2.2)
    void enable_metrics(bool enable);

    // Memory layout analysis
    const MemoryLayout::MemoryLayoutAnalyzer& get_memory_analyzer() const;

    // Batch operations
    BatchReadResult batch_read(std::vector<ReadRequest>& requests, DWORD process_id, bool optimize = true);

    template<typename T>
    std::vector<std::optional<T>> batch_read_typed(const std::vector<uint64_t>& addresses, DWORD process_id);

    std::vector<uint8_t> batch_read_range(uint64_t start_address, uint64_t end_address, size_t chunk_size, DWORD process_id);

    const BatchOperations& get_batch_operations() const;

    // Health monitoring
    void enable_health_monitoring(bool enable = true);
    bool is_health_monitoring_enabled() const;
    void run_health_checks();
    HealthStatus get_health_status() const;
    std::string get_health_summary() const;
    void start_automatic_health_monitoring(std::chrono::seconds interval = std::chrono::seconds(30));
    void stop_automatic_health_monitoring();
    const HealthMonitor& get_health_monitor() const;

    // Memory dumping utilities (v1.9)
    bool dump_memory_region(uint64_t start_address, uint64_t end_address,
                             const std::string& filename, DWORD process_id,
                             DumpFormat format);
    bool dump_module(const std::string& module_name, const std::string& filename,
                      DWORD process_id, DumpFormat format);
    bool create_memory_snapshot(DWORD process_id, const std::string& snapshot_dir);
    void print_hex_dump(uint64_t address, size_t size, DWORD process_id);
    std::vector<uint64_t> compare_memory_dumps(const std::string& file1,
                                                 const std::string& file2);

    // Memory diffing utilities (v2.1)
    std::vector<MemoryDiff> compare_memory_snapshots(const std::string& snapshot1,
                                                      const std::string& snapshot2);
    std::vector<uint64_t> find_changed_addresses(uint64_t start_address, uint64_t end_address,
                                                   DWORD process_id, std::chrono::milliseconds interval);
    std::vector<MemoryDiff> compare_memory_regions(uint64_t start_address, uint64_t end_address,
                                                     DWORD process_id, std::chrono::milliseconds interval);
    std::vector<uint64_t> find_memory_value(uint64_t start_address, uint64_t end_address,
                                              DWORD process_id, const std::vector<uint8_t>& value);
    template<typename T>
    std::vector<uint64_t> find_value_typed(uint64_t start_address, uint64_t end_address,
                                             DWORD process_id, T value);
    std::vector<uint64_t> filter_changed_addresses(const std::vector<uint64_t>& addresses,
                                                     DWORD process_id, std::chrono::milliseconds interval);
    const MemoryDiffer& get_memory_differ() const;

    // Rate limiting (v2.3)
    void enable_rate_limiting(bool enable);
    void set_rate_limit(size_t bytes_per_sec);
    [[nodiscard]] bool is_rate_limiting_enabled() const;
    [[nodiscard]] size_t get_rate_limit() const;

private:
    bool dump_memory_map();
    bool clean_fpga();

    // Metrics collector
    std::unique_ptr<Metrics::MetricsCollector> metrics_;

    // Memory cache
    std::unique_ptr<Cache::MemoryCache> cache_;

    // Memory layout analyzer
    std::unique_ptr<MemoryLayout::MemoryLayoutAnalyzer> memory_analyzer_;

    // Batch operations manager
    std::unique_ptr<BatchOperations> batch_ops_;

    // Health monitor
    std::unique_ptr<HealthMonitor> health_monitor_;

    // Memory differ
    std::unique_ptr<MemoryDiffer> memory_differ_;

    // Rate limiter (v2.3)
    std::unique_ptr<RateLimiter> rate_limiter_;
};

// Template implementations
template<typename T>
std::vector<uint64_t> DMA::find_value_typed(uint64_t start_address, uint64_t end_address,
                                              DWORD process_id, T value) {
    std::vector<uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    return find_memory_value(start_address, end_address, process_id, bytes);
}

} // namespace ArgoSentry