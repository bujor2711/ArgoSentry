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

    // Circuit Breaker (v3.0)
    class CircuitBreaker;
    enum class CircuitState;

    // Self-Healing System (v3.0)
    class SelfHealing;
    struct SelfHealingConfig;
    struct SelfHealingStats;

    // Pointer Chain Resolver (v3.1 - RE Tools)
    class PointerChainManager;

    // Value Freezer (v3.1 - RE Tools)
    class ValueFreezer;
    struct ValueFreezerStats;

    // Enhanced Pattern Scanner (v3.1 - RE Tools)
    class EnhancedPatternScanner;
    struct PatternScanStats;

    // Memory Structure Manager (v3.1 - RE Tools)
    class MemoryStructManager;

    // Module Enumerator (v3.1 - RE Tools FAZA 2)
    class ModuleEnumerator;
    struct ModuleInfo;

    // Builder pattern (v2.2)
    class DMABuilder;

    // Logging framework (v2.9)
    class Logger;
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
    /**
     * @brief Construct DMA with optional logger
     * @param use_memory_map Enable memory map (default: true)
     * @param logger Optional logger for operations (default: nullptr)
     * @since v2.9 - Added logger parameter
     */
    explicit DMA(bool use_memory_map = true, std::shared_ptr<Logger> logger = nullptr);
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

    // ✅ Public const getter for handle (safe read-only access)
    [[nodiscard]] const ArgoHandle& get_handle() const noexcept { return handle_; }

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

    // Circuit Breaker (v3.0)
    [[nodiscard]] CircuitBreaker* get_circuit_breaker() noexcept;
    [[nodiscard]] const CircuitBreaker* get_circuit_breaker() const noexcept;
    [[nodiscard]] CircuitState get_circuit_state() const noexcept;
    void trip_circuit_breaker() noexcept;
    void reset_circuit_breaker() noexcept;

    // Self-Healing System (v3.0)
    [[nodiscard]] SelfHealing* get_self_healing() noexcept;
    [[nodiscard]] const SelfHealing* get_self_healing() const noexcept;
    [[nodiscard]] SelfHealingStats get_self_healing_stats() const;
    void reset_self_healing_stats();

    // Pointer Chain Resolver (v3.1 - RE Tools)
    [[nodiscard]] PointerChainManager* get_pointer_chain_manager() noexcept;
    [[nodiscard]] const PointerChainManager* get_pointer_chain_manager() const noexcept;

    // Value Freezer (v3.1 - RE Tools)
    [[nodiscard]] ValueFreezer* create_value_freezer(DWORD process_id);
    void destroy_value_freezer(DWORD process_id);
    [[nodiscard]] ValueFreezer* get_value_freezer(DWORD process_id) noexcept;

    // Enhanced Pattern Scanner (v3.1 - RE Tools)
    [[nodiscard]] EnhancedPatternScanner* create_pattern_scanner(DWORD process_id);
    void destroy_pattern_scanner(DWORD process_id);
    [[nodiscard]] EnhancedPatternScanner* get_pattern_scanner(DWORD process_id) noexcept;

    // Memory Structure Manager (v3.1 - RE Tools)
    [[nodiscard]] MemoryStructManager* create_struct_manager(DWORD process_id);
    void destroy_struct_manager(DWORD process_id);
    [[nodiscard]] MemoryStructManager* get_struct_manager(DWORD process_id) noexcept;

    // Module Enumerator (v3.1 - RE Tools FAZA 2)
    [[nodiscard]] ModuleEnumerator* create_module_enumerator(DWORD process_id);
    void destroy_module_enumerator(DWORD process_id);
    [[nodiscard]] ModuleEnumerator* get_module_enumerator(DWORD process_id) noexcept;

private:
    bool dump_memory_map();
    bool clean_fpga();

    // ✅ FPGA DMA handle - now private to prevent misuse (can't be moved/stolen)
    ArgoHandle handle_;

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

    // Circuit breaker (v3.0)
    std::unique_ptr<CircuitBreaker> circuit_breaker_;

    // Self-healing system (v3.0)
    std::unique_ptr<SelfHealing> self_healing_;

    // Pointer chain manager (v3.1 - RE Tools)
    std::unique_ptr<PointerChainManager> pointer_chain_manager_;

    // Value freezers (v3.1 - RE Tools) - one per process
    std::map<DWORD, std::unique_ptr<ValueFreezer>> value_freezers_;
    mutable std::mutex value_freezers_mutex_;

    // Enhanced pattern scanners (v3.1 - RE Tools) - one per process
    std::map<DWORD, std::unique_ptr<EnhancedPatternScanner>> pattern_scanners_;
    mutable std::mutex pattern_scanners_mutex_;

    // Memory struct managers (v3.1 - RE Tools) - one per process
    std::map<DWORD, std::unique_ptr<MemoryStructManager>> struct_managers_;
    mutable std::mutex struct_managers_mutex_;

    // Module enumerators (v3.1 - RE Tools FAZA 2) - one per process
    std::map<DWORD, std::unique_ptr<ModuleEnumerator>> module_enumerators_;
    mutable std::mutex module_enumerators_mutex_;

    // Logger (v2.9)
    std::shared_ptr<Logger> logger_;
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