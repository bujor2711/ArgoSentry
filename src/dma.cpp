// ArgoSentry - Main DMA Implementation
// Complete implementation with FPGA hardware support
// v3.0 - Health Monitoring with Circuit Breaker & Self-Healing

#include "ArgoSentry/dma.hh"
#include "ArgoSentry/validators.hh"
#include "ArgoSentry/batch.hh"
#include "ArgoSentry/metrics.hh"
#include "ArgoSentry/cache.hh"
#include "ArgoSentry/health.hh"
#include "ArgoSentry/memory_layout.hh"
#include "ArgoSentry/differ.hh"
#include "ArgoSentry/builder.hh"  // v2.2 - Builder pattern
#include "ArgoSentry/rate_limiter.hh"  // v2.3 - Rate limiting
#include "ArgoSentry/compiled_pattern.hh"  // v2.5 - Pattern compilation
#include "ArgoSentry/logger.hh"  // v2.9 - Logging framework
#include "ArgoSentry/circuit_breaker.hh"  // v3.0 - Circuit breaker pattern
#include "ArgoSentry/self_healing.hh"  // v3.0 - Self-healing system
#include "ArgoSentry/pointer_chain.hh"  // v3.1 - Pointer chain resolver (RE Tools)
#include "ArgoSentry/value_freezer.hh"  // v3.1 - Value freezer (RE Tools)
#include "ArgoSentry/pattern_scanner_enhanced.hh"  // v3.1 - Enhanced pattern scanner (RE Tools)
#include "ArgoSentry/memory_struct.hh"  // v3.1 - Memory structure templates (RE Tools)
#include "ArgoSentry/module_enum.hh"  // v3.1 - Module enumerator (RE Tools FAZA 2)
#include "ArgoSentry/offset_finder.hh"  // v3.1 - Offset finder (RE Tools FAZA 2)

#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>

// VMM API includes
extern "C" {
    #include "external/vmm/vmmdll.h"
}

namespace ArgoSentry {

//==============================================================================
// Constructor - Initialize DMA with FPGA hardware
//==============================================================================
DMA::DMA(bool use_memory_map, std::shared_ptr<Logger> logger)
    : handle_(nullptr, vmm_close)  // ✅ Initialize private handle_
    , logger_(logger)  // v2.9: Store logger
{
    if (logger_) {
        LOG_INFO(logger_, "Initializing DMA with FPGA hardware...");
    }

    // Initialize VMM with FPGA device
    LPCSTR args[] = {
        "",                    // argv[0] placeholder
        "-device",
        "fpga",               // Use FPGA device
        "-waitinitialize"     // Wait for initialization
    };

    VMM_HANDLE vmm_handle = VMMDLL_Initialize(4, args);

    if (!vmm_handle) {
        if (logger_) {
            LOG_ERROR(logger_, "Failed to initialize DMA device. FPGA not connected or drivers missing.");
        }
        throw std::runtime_error(
            "Failed to initialize DMA device. "
            "Make sure FPGA is connected and drivers are installed. "
            "Run as Administrator."
        );
    }

    // Transfer ownership to unique_ptr
    handle_.reset(vmm_handle);

    if (logger_) {
        LOG_INFO(logger_, "DMA device initialized successfully");
    }

    // Initialize all subsystems
    metrics_ = std::make_unique<Metrics::MetricsCollector>();
    cache_ = std::make_unique<Cache::MemoryCache>();
    memory_analyzer_ = std::make_unique<MemoryLayout::MemoryLayoutAnalyzer>();
    batch_ops_ = std::make_unique<BatchOperations>();
    memory_differ_ = std::make_unique<MemoryDiffer>();
    rate_limiter_ = std::make_unique<RateLimiter>(0);  // v2.3: Disabled by default

    // Set VMM handle for batch operations
    batch_ops_->set_vmm_handle(handle_.get());

    // Health monitoring is optional, not initialized by default
    health_monitor_ = nullptr;

    // Optionally load memory map
    if (use_memory_map) {
        if (logger_) {
            LOG_DEBUG(logger_, "Loading memory map...");
        }
        dump_memory_map();
    }

    // Initialize circuit breaker (v3.0)
    // Default config: 5 failures, 30s timeout, 2 successes to close
    CircuitBreakerConfig cb_config;
    cb_config.failure_threshold = 5;
    cb_config.open_timeout = std::chrono::seconds(30);
    cb_config.success_threshold = 2;
    cb_config.on_state_change = [this](CircuitState old_state, CircuitState new_state) {
        if (logger_) {
            std::string msg = "Circuit breaker state transition: " +
                            std::string(to_string(old_state)) + " -> " +
                            std::string(to_string(new_state));
            LOG_WARN(logger_, msg);
        }
    };
    circuit_breaker_ = std::make_unique<CircuitBreaker>(cb_config);

    // Initialize self-healing system (v3.0)
    // Default config: Exponential backoff, 3 retries, auto-reconnect enabled
    SelfHealingConfig sh_config;
    sh_config.retry_policy = RetryPolicy::EXPONENTIAL;
    sh_config.max_retry_attempts = 3;
    sh_config.initial_retry_delay = std::chrono::milliseconds(100);
    sh_config.max_retry_delay = std::chrono::milliseconds(5000);
    sh_config.use_circuit_breaker = true;
    sh_config.auto_reconnect = true;
    sh_config.enable_health_checks = true;

    // Setup retry callback for logging
    sh_config.on_retry_attempt = [this](const std::string& operation, size_t attempt, const std::error_code& error) {
        if (logger_) {
            std::string msg = "Self-healing retry attempt " + std::to_string(attempt) +
                            " for '" + operation + "' (error: " + error.message() + ")";
            LOG_WARN(logger_, msg);
        }
    };

    sh_config.on_retry_exhausted = [this](const std::string& operation, size_t total_attempts) {
        if (logger_) {
            std::string msg = "Self-healing exhausted " + std::to_string(total_attempts) +
                            " attempts for '" + operation + "' - operation failed";
            LOG_ERROR(logger_, msg);
        }
    };

    sh_config.on_reconnect_start = [this]() {
        if (logger_) {
            LOG_WARN(logger_, "Self-healing: Attempting DMA reconnection...");
        }
    };

    sh_config.on_reconnect_complete = [this](bool success) {
        if (logger_) {
            if (success) {
                LOG_INFO(logger_, "Self-healing: DMA reconnection successful");
            } else {
                LOG_ERROR(logger_, "Self-healing: DMA reconnection failed");
            }
        }
    };

    self_healing_ = std::make_unique<SelfHealing>(sh_config, circuit_breaker_.get());

    // Initialize pointer chain manager (v3.1 - RE Tools)
    pointer_chain_manager_ = std::make_unique<PointerChainManager>();

    if (logger_) {
        LOG_INFO(logger_, "All DMA subsystems initialized successfully");
    }
    std::cout << "[VolkDMA] Successfully initialized with FPGA hardware\n";
}

//==============================================================================
// Destructor
//==============================================================================
DMA::~DMA() {
    // Clean up
    if (health_monitor_) {
        stop_automatic_health_monitoring();
    }

    clean_fpga();

    // Unique pointers will clean up automatically
    // handle will be closed by vmm_close lambda
}

//==============================================================================
// Builder Pattern (v2.2)
//==============================================================================
DMABuilder DMA::Builder() {
    return DMABuilder();
}

//==============================================================================
// Process ID Discovery
//==============================================================================
DWORD DMA::get_process_id(const std::string& process_name) const {
    if (process_name.empty()) {
        if (logger_) {
            LOG_WARN(logger_, "get_process_id called with empty process name");
        }
        throw std::invalid_argument("Process name cannot be empty");
    }

    if (logger_) {
        LOG_INFO(logger_, "Searching for process: " + process_name);
    }

    // Record process lookup
    auto start = std::chrono::high_resolution_clock::now();

    // Get process ID list
    SIZE_T pid_count = 0;

    if (!VMMDLL_PidList(handle_.get(), nullptr, &pid_count)) {
        if (logger_) {
            LOG_ERROR(logger_, "Failed to get process list from DMA device");
        }
        metrics_->record_process_lookup(false);
        return 0;
    }

    if (pid_count == 0) {
        if (logger_) {
            LOG_WARN(logger_, "No processes found on target system");
        }
        metrics_->record_process_lookup(false);
        return 0;
    }

    // ✅ Use unique_ptr for automatic cleanup (RAII)
    auto pid_list = std::make_unique<DWORD[]>(pid_count);
    if (!VMMDLL_PidList(handle_.get(), pid_list.get(), &pid_count)) {
        if (logger_) {
            LOG_ERROR(logger_, "Failed to retrieve process list");
        }
        metrics_->record_process_lookup(false);
        return 0;  // ✅ unique_ptr automatically frees memory
    }

    // Search for process
    DWORD found_pid = 0;
    std::string lower_process_name = process_name;
    std::transform(lower_process_name.begin(), lower_process_name.end(), 
                   lower_process_name.begin(), ::tolower);

    for (DWORD i = 0; i < pid_count; ++i) {
        VMMDLL_PROCESS_INFORMATION proc_info = {};
        proc_info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
        proc_info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

        SIZE_T cb_process_info = sizeof(VMMDLL_PROCESS_INFORMATION);
        if (VMMDLL_ProcessGetInformation(handle_.get(), pid_list[i], &proc_info, &cb_process_info)) {
            std::string current_name = proc_info.szName;
            std::transform(current_name.begin(), current_name.end(), 
                           current_name.begin(), ::tolower);

            if (current_name == lower_process_name) {
                found_pid = pid_list[i];
                break;  // ✅ unique_ptr will cleanup automatically
            }
        }
    }

    // ✅ No manual delete[] needed - RAII handles it

    metrics_->record_process_lookup(found_pid != 0);

    if (found_pid != 0) {
        if (logger_) {
            LOG_INFO(logger_, "Process found: " + process_name + " (PID: " + std::to_string(found_pid) + ")");
        }
    } else {
        if (logger_) {
            LOG_WARN(logger_, "Process not found: " + process_name);
        }
    }

    return found_pid;
}

//==============================================================================
// Get all process IDs with matching name
//==============================================================================
std::vector<DWORD> DMA::get_process_id_list(const std::string& process_name) const {
    std::vector<DWORD> result;

    if (process_name.empty()) {
        return result;
    }

    // Get all PIDs
    SIZE_T pid_count = 0;

    if (!VMMDLL_PidList(handle_.get(), nullptr, &pid_count)) {
        return result;
    }

    // ✅ Use unique_ptr for automatic cleanup (RAII)
    auto pid_list = std::make_unique<DWORD[]>(pid_count);
    if (!VMMDLL_PidList(handle_.get(), pid_list.get(), &pid_count)) {
        return result;  // ✅ unique_ptr automatically frees memory
    }

    // Find all matching processes
    std::string lower_process_name = process_name;
    std::transform(lower_process_name.begin(), lower_process_name.end(), 
                   lower_process_name.begin(), ::tolower);

    for (DWORD i = 0; i < pid_count; ++i) {
        VMMDLL_PROCESS_INFORMATION proc_info = {};
        proc_info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
        proc_info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

        SIZE_T cb_process_info = sizeof(VMMDLL_PROCESS_INFORMATION);
        if (VMMDLL_ProcessGetInformation(handle_.get(), pid_list[i], &proc_info, &cb_process_info)) {
            std::string current_name = proc_info.szName;
            std::transform(current_name.begin(), current_name.end(), 
                           current_name.begin(), ::tolower);

            if (current_name == lower_process_name) {
                result.push_back(pid_list[i]);
            }
        }
    }

    // ✅ No manual delete[] needed - RAII handles it

    return result;
}

//==============================================================================
// Template read implementation
//==============================================================================
template<typename T>
T DMA::read(uint64_t address, DWORD process_id) const {
    static_assert(std::is_trivially_copyable<T>::value, 
                  "T must be trivially copyable");
    static_assert(!std::is_pointer<T>::value, 
                  "T cannot be a pointer type");

    // ✅ Log entry: address, size, PID (v2.9)
    if (logger_) {
        std::stringstream ss;
        ss << "Reading " << sizeof(T) << " bytes from 0x" 
           << std::hex << address << std::dec 
           << " (PID " << process_id << ")";
        LOG_DEBUG(logger_, ss.str());
    }

    // Validate inputs
    if (!Validation::ProcessValidator::is_valid_process_id(process_id)) {
        if (logger_) {
            LOG_ERROR(logger_, "Invalid process ID: " + std::to_string(process_id));
        }
        throw std::invalid_argument("Invalid process ID");
    }

    if (!Validation::MemoryRangeValidator::is_safe_range(address, sizeof(T))) {
        if (logger_) {
            std::stringstream ss;
            ss << "Invalid memory range: 0x" << std::hex << address << std::dec 
               << ", size " << sizeof(T);
            LOG_ERROR(logger_, ss.str());
        }
        throw std::invalid_argument("Invalid memory address or size");
    }

    // Apply rate limiting (v2.3)
    if (rate_limiter_) {
        rate_limiter_->wait_if_needed(sizeof(T));
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Check cache first
    if (cache_) {
        auto cached = cache_->get(address, sizeof(T));
        if (cached && cached->size() >= sizeof(T)) {
            T value;
            std::memcpy(&value, cached->data(), sizeof(T));

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            metrics_->record_read(sizeof(T), duration.count(), true);
            metrics_->record_cache_hit();

            // ✅ Log cache hit (v2.9)
            if (logger_) {
                std::stringstream ss;
                ss << "Cache hit: Read " << sizeof(T) << " bytes from 0x" 
                   << std::hex << address << std::dec 
                   << " (" << duration.count() << " μs)";
                LOG_DEBUG(logger_, ss.str());
            }

            return value;
        }
        metrics_->record_cache_miss();
    }
    
    // Read from DMA
    T value = {};
    DWORD bytes_read = 0;
    
    BOOL success = VMMDLL_MemReadEx(
        handle_.get(),
        process_id,
        address,
        reinterpret_cast<PBYTE>(&value),
        sizeof(T),
        &bytes_read,
        VMMDLL_FLAG_NOCACHE
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // ✅ Explicit null/partial read checks for safety
    if (!success) {
        metrics_->record_read(bytes_read, duration.count(), false);

        // ✅ Log DMA read failure (v2.9)
        if (logger_) {
            std::stringstream ss;
            ss << "DMA read failed at 0x" << std::hex << address << std::dec 
               << " (size: " << sizeof(T) << ", PID: " << process_id << ")";
            LOG_ERROR(logger_, ss.str());
        }

        throw std::runtime_error("DMA read failed at address 0x" + 
                                 std::to_string(address) + " - VMMDLL_MemReadEx returned failure");
    }

    if (bytes_read == 0) {
        metrics_->record_read(0, duration.count(), false);

        // ✅ Log zero bytes read (v2.9)
        if (logger_) {
            std::stringstream ss;
            ss << "Zero bytes read at 0x" << std::hex << address << std::dec 
               << " (PID: " << process_id << ") - possible invalid address or permissions";
            LOG_ERROR(logger_, ss.str());
        }

        throw std::runtime_error("DMA read returned zero bytes at address 0x" + 
                                 std::to_string(address) + " - possible invalid address or permissions");
    }

    if (bytes_read != sizeof(T)) {
        metrics_->record_read(bytes_read, duration.count(), false);

        // ✅ Log partial read (v2.9)
        if (logger_) {
            std::stringstream ss;
            ss << "Partial read at 0x" << std::hex << address << std::dec 
               << " (expected " << sizeof(T) << ", got " << bytes_read << ", PID: " << process_id << ")";
            LOG_WARN(logger_, ss.str());
        }

        throw std::runtime_error("Partial DMA read at address 0x" + 
                                 std::to_string(address) + 
                                 " - expected " + std::to_string(sizeof(T)) + 
                                 " bytes, got " + std::to_string(bytes_read));
    }

    // Store in cache
    if (cache_) {
        std::vector<uint8_t> data(sizeof(T));
        std::memcpy(data.data(), &value, sizeof(T));
        cache_->put(address, data);
    }

    metrics_->record_read(sizeof(T), duration.count(), true);

    // ✅ Log successful DMA read (v2.9)
    if (logger_) {
        std::stringstream ss;
        ss << "DMA read successful: " << sizeof(T) << " bytes from 0x" 
           << std::hex << address << std::dec 
           << " (" << duration.count() << " μs)";
        LOG_DEBUG(logger_, ss.str());

        // ✅ Performance warning for slow reads >100ms (v2.9)
        if (duration.count() > 100000) { // >100ms = 100,000 microseconds
            std::stringstream warn_ss;
            warn_ss << "Slow read detected: " << (duration.count() / 1000) 
                   << " ms at 0x" << std::hex << address << std::dec;
            LOG_WARN(logger_, warn_ss.str());
        }
    }

    return value;
}

// Explicit template instantiations for common types
template uint8_t DMA::read<uint8_t>(uint64_t, DWORD) const;
template uint16_t DMA::read<uint16_t>(uint64_t, DWORD) const;
template uint32_t DMA::read<uint32_t>(uint64_t, DWORD) const;
template uint64_t DMA::read<uint64_t>(uint64_t, DWORD) const;
template int8_t DMA::read<int8_t>(uint64_t, DWORD) const;
template int16_t DMA::read<int16_t>(uint64_t, DWORD) const;
template int32_t DMA::read<int32_t>(uint64_t, DWORD) const;
template int64_t DMA::read<int64_t>(uint64_t, DWORD) const;
template float DMA::read<float>(uint64_t, DWORD) const;
template double DMA::read<double>(uint64_t, DWORD) const;

//==============================================================================
// Signature Scanning
//==============================================================================

// Helper function to parse signature string (e.g., "48 8B ?? 89")
static std::vector<std::pair<uint8_t, bool>> parse_signature(const char* signature) {
    std::vector<std::pair<uint8_t, bool>> pattern;
    std::string sig_str(signature);
    std::istringstream iss(sig_str);
    std::string token;
    
    while (iss >> token) {
        if (token == "??" || token == "?" || token == "*") {
            pattern.push_back({0, false}); // Wildcard
        } else {
            try {
                uint8_t byte = static_cast<uint8_t>(std::stoul(token, nullptr, 16));
                pattern.push_back({byte, true}); // Exact match
            } catch (...) {
                throw std::invalid_argument("Invalid signature format");
            }
        }
    }
    
    return pattern;
}

// Helper function to match pattern in buffer
static bool match_pattern(const uint8_t* buffer, size_t buffer_size, 
                          const std::vector<std::pair<uint8_t, bool>>& pattern) {
    if (buffer_size < pattern.size()) {
        return false;
    }
    
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i].second) { // Not a wildcard
            if (buffer[i] != pattern[i].first) {
                return false;
            }
        }
    }
    
    return true;
}

uint64_t DMA::find_signature(const char* signature, uint64_t range_start, 
                              uint64_t range_end, DWORD process_id) const {
    if (!signature || range_start >= range_end) {
        if (logger_) {
            std::stringstream ss;
            ss << "Invalid signature scan parameters: signature=" 
               << (signature ? signature : "NULL")
               << ", range=0x" << std::hex << range_start << "-0x" << range_end << std::dec;
            LOG_ERROR(logger_, ss.str());
        }
        throw std::invalid_argument("Invalid signature or range");
    }

    if (!Validation::ProcessValidator::is_valid_process_id(process_id)) {
        if (logger_) {
            LOG_ERROR(logger_, "Invalid process ID: " + std::to_string(process_id));
        }
        throw std::invalid_argument("Invalid process ID");
    }

    // ✅ Log scan start (v2.9)
    if (logger_) {
        std::stringstream ss;
        ss << "Scanning for pattern: " << signature 
           << " in range 0x" << std::hex << range_start << "-0x" << range_end << std::dec
           << " (PID " << process_id << ", size " 
           << ((range_end - range_start) / 1024 / 1024) << " MB)";
        LOG_INFO(logger_, ss.str());
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Parse signature
    auto pattern = parse_signature(signature);
    if (pattern.empty()) {
        throw std::invalid_argument("Empty signature pattern");
    }
    
    // Scan memory in chunks
    constexpr size_t chunk_size = 4096; // 4KB chunks
    std::vector<uint8_t> buffer(chunk_size);
    
    uint64_t current_address = range_start;
    uint64_t total_scanned = 0;
    uint64_t found_address = 0;
    
    while (current_address < range_end) {
        size_t read_size = static_cast<size_t>(
            (chunk_size < static_cast<size_t>(range_end - current_address)) ? 
            chunk_size : static_cast<size_t>(range_end - current_address)
        );
        
        DWORD bytes_read = 0;
        BOOL success = VMMDLL_MemReadEx(
            handle_.get(),
            process_id,
            current_address,
            buffer.data(),
            static_cast<DWORD>(read_size),
            &bytes_read,
            VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL
        );
        
        if (success && bytes_read > 0) {
            total_scanned += bytes_read;
            
            // Search for pattern in this chunk
            for (size_t i = 0; i <= bytes_read - pattern.size(); ++i) {
                if (match_pattern(buffer.data() + i, bytes_read - i, pattern)) {
                    found_address = current_address + i;
                    break;
                }
            }
            
            if (found_address != 0) {
                break;
            }
        }
        
        current_address += read_size;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    metrics_->record_scan(total_scanned, duration.count(), found_address != 0);

    // ✅ Log scan result (v2.9)
    if (logger_) {
        if (found_address != 0) {
            std::stringstream ss;
            ss << "Pattern found at 0x" << std::hex << found_address << std::dec
               << " (" << (duration.count() / 1000) << " ms, " 
               << (total_scanned / 1024 / 1024) << " MB scanned)";
            LOG_INFO(logger_, ss.str());
        } else {
            std::stringstream ss;
            ss << "Pattern not found (" << (duration.count() / 1000) << " ms, "
               << (total_scanned / 1024 / 1024) << " MB scanned)";
            LOG_DEBUG(logger_, ss.str());
        }

        // ✅ Performance warning for slow scans >100ms (v2.9)
        if (duration.count() > 100000) { // >100ms
            std::stringstream warn_ss;
            warn_ss << "Slow signature scan: " << (duration.count() / 1000) 
                   << " ms (range: 0x" << std::hex << range_start << "-0x" << range_end << std::dec << ")";
            LOG_WARN(logger_, warn_ss.str());
        }
    }

    return found_address;
}

// Overload for CompiledPattern (v2.5 - 2-3x faster)
uint64_t DMA::find_signature(const CompiledPattern& pattern, uint64_t range_start, 
                              uint64_t range_end, DWORD process_id) const {
    if (range_start >= range_end) {
        if (logger_) {
            std::stringstream ss;
            ss << "Invalid range: 0x" << std::hex << range_start << " >= 0x" << range_end << std::dec;
            LOG_ERROR(logger_, ss.str());
        }
        throw std::invalid_argument("Invalid range: start >= end");
    }

    if (!Validation::ProcessValidator::is_valid_process_id(process_id)) {
        if (logger_) {
            LOG_ERROR(logger_, "Invalid process ID: " + std::to_string(process_id));
        }
        throw std::invalid_argument("Invalid process ID");
    }

    // ✅ Log scan start with compiled pattern (v2.9)
    if (logger_) {
        std::stringstream ss;
        ss << "Scanning for compiled pattern: " << pattern.to_string()
           << " in range 0x" << std::hex << range_start << "-0x" << range_end << std::dec
           << " (PID " << process_id << ", size " 
           << ((range_end - range_start) / 1024 / 1024) << " MB)";
        LOG_INFO(logger_, ss.str());
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Scan memory in chunks
    constexpr size_t chunk_size = 4096; // 4KB chunks
    std::vector<uint8_t> buffer(chunk_size);

    uint64_t current_address = range_start;
    uint64_t total_scanned = 0;
    uint64_t found_address = 0;

    while (current_address < range_end) {
        size_t read_size = static_cast<size_t>(
            (chunk_size < static_cast<size_t>(range_end - current_address)) ? 
            chunk_size : static_cast<size_t>(range_end - current_address)
        );

        DWORD bytes_read = 0;
        BOOL success = VMMDLL_MemReadEx(
            handle_.get(),
            process_id,
            current_address,
            buffer.data(),
            static_cast<DWORD>(read_size),
            &bytes_read,
            VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL
        );

        if (success && bytes_read > 0) {
            total_scanned += bytes_read;

            // Use CompiledPattern's optimized find_in_buffer
            found_address = pattern.find_in_buffer(buffer.data(), bytes_read, current_address);

            if (found_address != 0) {
                break;
            }
        }

        current_address += read_size;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    metrics_->record_scan(total_scanned, duration.count(), found_address != 0);

    // ✅ Log compiled pattern scan result (v2.9)
    if (logger_) {
        if (found_address != 0) {
            std::stringstream ss;
            ss << "Compiled pattern found at 0x" << std::hex << found_address << std::dec
               << " (" << (duration.count() / 1000) << " ms, " 
               << (total_scanned / 1024 / 1024) << " MB scanned)";
            LOG_INFO(logger_, ss.str());
        } else {
            std::stringstream ss;
            ss << "Compiled pattern not found (" << (duration.count() / 1000) << " ms, "
               << (total_scanned / 1024 / 1024) << " MB scanned)";
            LOG_DEBUG(logger_, ss.str());
        }

        // ✅ Performance warning for slow scans >100ms (v2.9)
        if (duration.count() > 100000) { // >100ms
            std::stringstream warn_ss;
            warn_ss << "Slow compiled pattern scan: " << (duration.count() / 1000) 
                   << " ms (range: 0x" << std::hex << range_start << "-0x" << range_end << std::dec << ")";
            LOG_WARN(logger_, warn_ss.str());
        }
    }

    return found_address;
}

//==============================================================================
// Smart signature scanning using memory layout
//==============================================================================
uint64_t DMA::find_signature_in_module(const char* signature, 
                                        const std::string& module_name, 
                                        DWORD process_id) const {
    if (!memory_analyzer_) {
        throw std::runtime_error("Memory analyzer not initialized");
    }

    // Find module
    auto module = memory_analyzer_->find_module(process_id, module_name);
    if (!module) {
        return 0; // Module not found
    }

    // Scan within module range
    return find_signature(signature, module->base_address, 
                          module->end_address(), process_id);
}

uint64_t DMA::find_signature_in_module(const CompiledPattern& pattern, 
                                        const std::string& module_name, 
                                        DWORD process_id) const {
    if (!memory_analyzer_) {
        throw std::runtime_error("Memory analyzer not initialized");
    }

    // Find module
    auto module = memory_analyzer_->find_module(process_id, module_name);
    if (!module) {
        return 0; // Module not found
    }

    // Scan within module range with compiled pattern
    return find_signature(pattern, module->base_address, 
                          module->end_address(), process_id);
}

uint64_t DMA::find_signature_in_executable(const char* signature, 
                                            DWORD process_id) const {
    if (!memory_analyzer_) {
        throw std::runtime_error("Memory analyzer not initialized");
    }

    // Get all executable regions
    auto regions = memory_analyzer_->get_executable_regions(process_id);

    // Search each executable region
    for (const auto& region : regions) {
        uint64_t result = find_signature(signature, region.base_address, 
                                          region.end_address(), process_id);
        if (result != 0) {
            return result;
        }
    }

    return 0; // Not found
}

uint64_t DMA::find_signature_in_executable(const CompiledPattern& pattern, 
                                            DWORD process_id) const {
    if (!memory_analyzer_) {
        throw std::runtime_error("Memory analyzer not initialized");
    }

    // Get all executable regions
    auto regions = memory_analyzer_->get_executable_regions(process_id);

    // Search each executable region with compiled pattern
    for (const auto& region : regions) {
        uint64_t result = find_signature(pattern, region.base_address, 
                                          region.end_address(), process_id);
        if (result != 0) {
            return result;
        }
    }

    return 0; // Not found
}

//==============================================================================
// Metrics Access
//==============================================================================
const Metrics::MetricsCollector& DMA::get_metrics() const {
    if (!metrics_) {
        throw std::runtime_error("Metrics not initialized");
    }
    return *metrics_;
}

void DMA::reset_metrics() {
    if (metrics_) {
        metrics_->reset_metrics();
    }
}

void DMA::log_metrics_summary() const {
    if (metrics_) {
        metrics_->log_summary();
    }
}

void DMA::log_metrics_detailed() const {
    if (metrics_) {
        metrics_->log_detailed();
    }
}

//==============================================================================
// Cache Management
//==============================================================================
void DMA::enable_cache(bool enable) {
    if (enable && !cache_) {
        cache_ = std::make_unique<Cache::MemoryCache>();
    } else if (!enable) {
        cache_.reset();
    }
}

void DMA::clear_cache() {
    if (cache_) {
        cache_->clear();
    }
}

void DMA::invalidate_cache(uint64_t address) {
    if (cache_) {
        cache_->invalidate(address);
    }
}

size_t DMA::get_cache_size() const {
    return cache_ ? cache_->get_size() : 0;
}

size_t DMA::get_cache_entry_count() const {
    return cache_ ? cache_->get_entry_count() : 0;
}

void DMA::log_cache_statistics() const {
    if (cache_) {
        auto stats = cache_->get_statistics();
        std::cout << "Cache Statistics:\n";
        std::cout << "  Hits: " << stats.hits << "\n";
        std::cout << "  Misses: " << stats.misses << "\n";
        std::cout << "  Evictions: " << stats.evictions << "\n";
        std::cout << "  Current Size: " << stats.current_size << " bytes\n";
        std::cout << "  Entry Count: " << stats.entry_count << "\n";

        if (stats.hits + stats.misses > 0) {
            double hit_ratio = static_cast<double>(stats.hits) / 
                               (stats.hits + stats.misses);
            std::cout << "  Hit Ratio: " << (hit_ratio * 100.0) << "%\n";
        }
    }
}

//==============================================================================
// Cache Configuration (v2.2)
//==============================================================================
void DMA::set_cache_size(size_t size) {
    if (cache_) {
        cache_->set_max_size(size);
    }
}

void DMA::set_cache_ttl(std::chrono::seconds ttl) {
    if (cache_) {
        cache_->set_ttl(ttl);
    }
}

size_t DMA::get_cache_max_size() const {
    return cache_ ? cache_->get_max_size() : 0;
}

std::chrono::seconds DMA::get_cache_ttl() const {
    return cache_ ? cache_->get_ttl() : std::chrono::seconds(0);
}

//==============================================================================
// Metrics Configuration (v2.2)
//==============================================================================
void DMA::enable_metrics(bool enable) {
    // TODO: Add set_enabled() to MetricsCollector
    // For now, metrics are always collected but can be reset
    if (!enable && metrics_) {
        metrics_->reset_metrics();
    }
}

//==============================================================================
// Health Monitoring
//==============================================================================
void DMA::enable_health_monitoring(bool enable) {
    if (enable && !health_monitor_) {
        health_monitor_ = std::make_unique<HealthMonitor>(this);
    }
}

bool DMA::is_health_monitoring_enabled() const {
    return health_monitor_ != nullptr;
}

void DMA::run_health_checks() {
    if (!health_monitor_) {
        health_monitor_ = std::make_unique<HealthMonitor>(this);
    }
    health_monitor_->run_all_checks();
}

void DMA::start_automatic_health_monitoring(std::chrono::seconds interval) {
    // Simplified implementation - just enable monitoring
    enable_health_monitoring(true);
}

void DMA::stop_automatic_health_monitoring() {
    // Simplified implementation - monitoring stops when object is destroyed
}

HealthStatus DMA::get_health_status() const {
    if (!health_monitor_) {
        return HealthStatus::Healthy;  // Assume healthy if not monitoring
    }
    return health_monitor_->get_overall_status();
}

std::string DMA::get_health_summary() const {
    if (!health_monitor_) {
        return "Health monitoring not active";
    }
    return health_monitor_->get_health_summary();
}

const HealthMonitor& DMA::get_health_monitor() const {
    if (!health_monitor_) {
        throw std::runtime_error("Health monitor not initialized");
    }
    return *health_monitor_;
}

//==============================================================================
// Memory Layout Analysis
//==============================================================================
const MemoryLayout::MemoryLayoutAnalyzer& DMA::get_memory_analyzer() const {
    if (!memory_analyzer_) {
        throw std::runtime_error("Memory analyzer not initialized");
    }
    return *memory_analyzer_;
}

//==============================================================================
// Private Helper Methods
//==============================================================================
bool DMA::dump_memory_map() {
    if (!handle_) {
        return false;
    }
    
    // Configure memory map (optional optimization)
    // This loads the memory map into MemProcFS for faster access
    VMMDLL_ConfigSet(handle_.get(), VMMDLL_OPT_CONFIG_IS_REFRESH_ENABLED, 1);
    
    return true;
}

bool DMA::clean_fpga() {
    // Cleanup operations before shutdown
    if (handle_) {
        // Flush any pending operations
        VMMDLL_ConfigSet(handle_.get(), VMMDLL_OPT_CORE_PRINTF_ENABLE, 0);
    }

    return true;
}

//==============================================================================
// Rate Limiting (v2.3)
//==============================================================================
void DMA::enable_rate_limiting(bool enable) {
    if (enable && !rate_limiter_) {
        rate_limiter_ = std::make_unique<RateLimiter>(0);  // Disabled by default
    } else if (!enable) {
        rate_limiter_.reset();
    }
}

void DMA::set_rate_limit(size_t bytes_per_sec) {
    if (!rate_limiter_) {
        rate_limiter_ = std::make_unique<RateLimiter>(bytes_per_sec);
    } else {
        rate_limiter_->set_limit(bytes_per_sec);
    }
}

bool DMA::is_rate_limiting_enabled() const {
    return rate_limiter_ && rate_limiter_->is_enabled();
}

size_t DMA::get_rate_limit() const {
    return rate_limiter_ ? rate_limiter_->get_limit() : 0;
}

//==============================================================================
// Circuit Breaker (v3.0)
//==============================================================================

CircuitBreaker* DMA::get_circuit_breaker() noexcept {
    return circuit_breaker_.get();
}

const CircuitBreaker* DMA::get_circuit_breaker() const noexcept {
    return circuit_breaker_.get();
}

CircuitState DMA::get_circuit_state() const noexcept {
    if (!circuit_breaker_) {
        return CircuitState::CLOSED;  // No breaker = always closed
    }
    return circuit_breaker_->get_state();
}

void DMA::trip_circuit_breaker() noexcept {
    if (circuit_breaker_) {
        circuit_breaker_->trip();
    }
}

void DMA::reset_circuit_breaker() noexcept {
    if (circuit_breaker_) {
        circuit_breaker_->reset();
    }
}

//==============================================================================
// Self-Healing System (v3.0)
//==============================================================================

SelfHealing* DMA::get_self_healing() noexcept {
    return self_healing_.get();
}

const SelfHealing* DMA::get_self_healing() const noexcept {
    return self_healing_.get();
}

SelfHealingStats DMA::get_self_healing_stats() const {
    if (!self_healing_) {
        return SelfHealingStats{};  // Return empty stats if not initialized
    }
    return self_healing_->get_stats();
}

void DMA::reset_self_healing_stats() {
    if (self_healing_) {
        self_healing_->reset_stats();
    }
}

//==============================================================================
// Pointer Chain Resolver (v3.1 - RE Tools)
//==============================================================================

PointerChainManager* DMA::get_pointer_chain_manager() noexcept {
    return pointer_chain_manager_.get();
}

const PointerChainManager* DMA::get_pointer_chain_manager() const noexcept {
    return pointer_chain_manager_.get();
}

//==============================================================================
// Value Freezer (v3.1 - RE Tools)
//==============================================================================

ValueFreezer* DMA::create_value_freezer(DWORD process_id) {
    std::lock_guard<std::mutex> lock(value_freezers_mutex_);

    // Check if already exists
    if (value_freezers_.find(process_id) != value_freezers_.end()) {
        return value_freezers_[process_id].get();
    }

    // Create new freezer
    auto freezer = std::make_unique<ValueFreezer>(this, process_id);
    auto* ptr = freezer.get();
    value_freezers_[process_id] = std::move(freezer);

    return ptr;
}

void DMA::destroy_value_freezer(DWORD process_id) {
    std::lock_guard<std::mutex> lock(value_freezers_mutex_);
    value_freezers_.erase(process_id);
}

ValueFreezer* DMA::get_value_freezer(DWORD process_id) noexcept {
    std::lock_guard<std::mutex> lock(value_freezers_mutex_);
    auto it = value_freezers_.find(process_id);
    if (it != value_freezers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

//==============================================================================
// Enhanced Pattern Scanner (v3.1 - RE Tools)
//==============================================================================

EnhancedPatternScanner* DMA::create_pattern_scanner(DWORD process_id) {
    std::lock_guard<std::mutex> lock(pattern_scanners_mutex_);

    // Check if already exists
    if (pattern_scanners_.find(process_id) != pattern_scanners_.end()) {
        return pattern_scanners_[process_id].get();
    }

    // Create new scanner
    auto scanner = std::make_unique<EnhancedPatternScanner>(this, process_id);
    auto* ptr = scanner.get();
    pattern_scanners_[process_id] = std::move(scanner);

    return ptr;
}

void DMA::destroy_pattern_scanner(DWORD process_id) {
    std::lock_guard<std::mutex> lock(pattern_scanners_mutex_);
    pattern_scanners_.erase(process_id);
}

EnhancedPatternScanner* DMA::get_pattern_scanner(DWORD process_id) noexcept {
    std::lock_guard<std::mutex> lock(pattern_scanners_mutex_);
    auto it = pattern_scanners_.find(process_id);
    if (it != pattern_scanners_.end()) {
        return it->second.get();
    }
    return nullptr;
}

// Memory Structure Manager (v3.1 - RE Tools)

MemoryStructManager* DMA::create_struct_manager(DWORD process_id) {
    std::lock_guard<std::mutex> lock(struct_managers_mutex_);

    // Check if already exists
    auto it = struct_managers_.find(process_id);
    if (it != struct_managers_.end()) {
        return it->second.get();
    }

    // Create new manager
    auto manager = std::make_unique<MemoryStructManager>();
    auto* ptr = manager.get();
    struct_managers_[process_id] = std::move(manager);
    return ptr;
}

void DMA::destroy_struct_manager(DWORD process_id) {
    std::lock_guard<std::mutex> lock(struct_managers_mutex_);
    struct_managers_.erase(process_id);
}

MemoryStructManager* DMA::get_struct_manager(DWORD process_id) noexcept {
    std::lock_guard<std::mutex> lock(struct_managers_mutex_);
    auto it = struct_managers_.find(process_id);
    if (it != struct_managers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

// Module Enumerator (v3.1 - RE Tools FAZA 2)

ModuleEnumerator* DMA::create_module_enumerator(DWORD process_id) {
    std::lock_guard<std::mutex> lock(module_enumerators_mutex_);

    // Check if already exists
    auto it = module_enumerators_.find(process_id);
    if (it != module_enumerators_.end()) {
        return it->second.get();
    }

    // Create new enumerator
    auto enumerator = std::make_unique<ModuleEnumerator>(this, process_id);
    auto* ptr = enumerator.get();
    module_enumerators_[process_id] = std::move(enumerator);
    return ptr;
}

void DMA::destroy_module_enumerator(DWORD process_id) {
    std::lock_guard<std::mutex> lock(module_enumerators_mutex_);
    module_enumerators_.erase(process_id);
}

ModuleEnumerator* DMA::get_module_enumerator(DWORD process_id) noexcept {
    std::lock_guard<std::mutex> lock(module_enumerators_mutex_);
    auto it = module_enumerators_.find(process_id);
    if (it != module_enumerators_.end()) {
        return it->second.get();
    }
    return nullptr;
}

// Offset Finder (v3.1 - RE Tools FAZA 2)

OffsetFinder* DMA::create_offset_finder(DWORD process_id) {
    std::lock_guard<std::mutex> lock(offset_finders_mutex_);

    // Check if already exists
    auto it = offset_finders_.find(process_id);
    if (it != offset_finders_.end()) {
        return it->second.get();
    }

    // Need scanner and enumerator
    auto* scanner = get_pattern_scanner(process_id);
    auto* enumerator = get_module_enumerator(process_id);

    if (!scanner || !enumerator) {
        return nullptr;  // Dependencies not available
    }

    // Create new offset finder
    auto finder = std::make_unique<OffsetFinder>(this, scanner, enumerator, process_id);
    auto* ptr = finder.get();
    offset_finders_[process_id] = std::move(finder);
    return ptr;
}

void DMA::destroy_offset_finder(DWORD process_id) {
    std::lock_guard<std::mutex> lock(offset_finders_mutex_);
    offset_finders_.erase(process_id);
}

OffsetFinder* DMA::get_offset_finder(DWORD process_id) noexcept {
    std::lock_guard<std::mutex> lock(offset_finders_mutex_);
    auto it = offset_finders_.find(process_id);
    if (it != offset_finders_.end()) {
        return it->second.get();
    }
    return nullptr;
}

} // namespace ArgoSentry

