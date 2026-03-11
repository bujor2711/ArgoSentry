# 📚 VolkDMA API Reference

Complete documentation for all public APIs in VolkDMA v1.9.

---

## 📋 **Table of Contents**

1. [DMA Class](#dma-class) - Core DMA operations
2. [Validation](#validation) - Input validation (v1.1)
3. [Metrics](#metrics) - Performance tracking (v1.2)
4. [Configuration](#configuration) - Runtime settings (v1.4)
5. [Memory Cache](#memory-cache) - Caching system (v1.5)
6. [Memory Layout](#memory-layout) - Memory analysis (v1.6)
7. [Batch Operations](#batch-operations) - Multi-address reads (v1.7)
8. [Health Monitoring](#health-monitoring) - System health (v1.8)
9. [Memory Dumper](#memory-dumper) - Memory export (v1.9)

---

## 🔷 **DMA Class**

Main class for all DMA operations. Defined in `<VolkDMA/dma.hh>`.

```cpp
namespace VolkDMA {
    class DMA {
        // Core operations
        // Memory reading
        // Signature scanning
        // Process enumeration
    };
}
```

### **Constructor**

```cpp
DMA();
```

**Description:** Creates a DMA instance (does not initialize hardware yet).

**Example:**
```cpp
VolkDMA::DMA dma; // Create instance
```

---

### **initialize()**

```cpp
bool initialize();
```

**Description:** Initializes connection to FPGA hardware.

**Returns:**
- `true` - Successfully connected to FPGA
- `false` - Failed to connect (FPGA not found, driver issue, hardware problem)

**Example:**
```cpp
if (!dma.initialize()) {
    std::cerr << "Failed to initialize DMA\n";
    return 1;
}
```

**Thread Safety:** ✅ Thread-safe  
**Performance:** ~10-50ms (one-time initialization cost)

**Error Conditions:**
- FPGA device not connected
- FPGA drivers not installed
- Insufficient permissions
- Hardware malfunction

---

### **read<T>()**

```cpp
template<typename T>
T read(uint64_t address, DWORD process_id) const;
```

**Description:** Reads a value of type `T` from memory at `address` in process `process_id`.

**Template Parameters:**
- `T` - Type to read (must be trivially copyable)

**Parameters:**
- `address` - Memory address to read from (64-bit)
- `process_id` - Target process ID (must be valid)

**Returns:** Value of type `T` read from memory

**Supported Types:**
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- `int8_t`, `int16_t`, `int32_t`, `int64_t`
- `float`, `double`
- Custom structs (must be trivially copyable)

**Example:**
```cpp
// Read integer
uint32_t health = dma.read<uint32_t>(0x7FF700123456, pid);

// Read float
float position_x = dma.read<float>(0x7FF700123460, pid);

// Read struct
struct Vec3 { float x, y, z; };
Vec3 pos = dma.read<Vec3>(0x7FF700123464, pid);
```

**Throws:** `std::runtime_error` if read fails

**Thread Safety:** ✅ Thread-safe  
**Performance:** ~0.1-1ms per read (with cache: 0.001ms)

---

### **read_bytes()**

```cpp
std::vector<uint8_t> read_bytes(uint64_t address, size_t size, DWORD process_id) const;
```

**Description:** Reads raw bytes from memory.

**Parameters:**
- `address` - Start address
- `size` - Number of bytes to read
- `process_id` - Target process ID

**Returns:** Vector of bytes read

**Example:**
```cpp
auto data = dma.read_bytes(0x7FF700000000, 256, pid);
for (uint8_t byte : data) {
    std::cout << std::hex << (int)byte << " ";
}
```

**Throws:** `std::runtime_error` if read fails

**Thread Safety:** ✅ Thread-safe  
**Performance:** ~0.1-1ms per read

---

### **find_signature()**

```cpp
uint64_t find_signature(const char* signature,
                       uint64_t range_start,
                       uint64_t range_end,
                       DWORD process_id) const;
```

**Description:** Searches for a byte pattern in memory range.

**Parameters:**
- `signature` - Pattern string (hex bytes with spaces, `?` for wildcards)
- `range_start` - Start of search range (inclusive)
- `range_end` - End of search range (exclusive)
- `process_id` - Target process ID

**Returns:**
- Address where pattern found (64-bit address)
- `0` if pattern not found

**Pattern Format:**
```cpp
"48 8B 05 ? ? ? ?"  // Spaces between bytes, ? = wildcard
"E8 ? ? ? ? 90"     // Call instruction + NOP
```

**Example:**
```cpp
// Find entity list pattern
uint64_t addr = dma.find_signature(
    "48 8B 0D ? ? ? ? 48 85 C9",
    0x7FF700000000,  // Start of .text section
    0x7FF701000000,  // End of .text section
    pid
);

if (addr != 0) {
    std::cout << "Pattern found at: 0x" << std::hex << addr << "\n";
}
```

**Throws:** `std::runtime_error` if validation fails

**Thread Safety:** ✅ Thread-safe  
**Performance:** ~50-500ms (depends on range size, reduced 80-90% with v1.6 smart scanning)

---

### **find_signature_in_module()** (v1.6)

```cpp
uint64_t find_signature_in_module(const char* signature,
                                 const char* module_name,
                                 DWORD process_id) const;
```

**Description:** Scans for pattern in specific module only (much faster).

**Parameters:**
- `signature` - Pattern string
- `module_name` - Module name (e.g., "game.exe", "client.dll")
- `process_id` - Target process ID

**Returns:** Address where found, or `0`

**Example:**
```cpp
// Scan only in client.dll (10x faster)
uint64_t addr = dma.find_signature_in_module(
    "48 8B 05 ? ? ? ?",
    "client.dll",
    pid
);
```

**Performance:** ~5-50ms (80-90% faster than full scan)

---

### **find_signature_in_executable()** (v1.6)

```cpp
uint64_t find_signature_in_executable(const char* signature,
                                     DWORD process_id) const;
```

**Description:** Scans only executable regions (skips data, heap, stack).

**Parameters:**
- `signature` - Pattern string
- `process_id` - Target process ID

**Returns:** Address where found, or `0`

**Example:**
```cpp
// Smart scan (only code regions)
uint64_t addr = dma.find_signature_in_executable(
    "E8 ? ? ? ? 48 8B",
    pid
);
```

**Performance:** ~10-100ms (80-90% faster than full scan)

---

### **get_process_id()**

```cpp
DWORD get_process_id(const char* process_name) const;
```

**Description:** Finds process ID by name.

**Parameters:**
- `process_name` - Process name (case-insensitive, e.g., "notepad.exe")

**Returns:**
- Process ID (DWORD) if found
- `0` if not found

**Example:**
```cpp
DWORD pid = dma.get_process_id("csgo.exe");
if (pid == 0) {
    std::cerr << "Process not found!\n";
}
```

**Thread Safety:** ✅ Thread-safe  
**Performance:** ~1-10ms

---

### **batch_read()** (v1.7)

```cpp
BatchReadResult batch_read(std::vector<ReadRequest>& requests, DWORD process_id);
```

**Description:** Reads multiple addresses in a single optimized operation (50-80% faster).

**Parameters:**
- `requests` - Vector of `ReadRequest` structs
- `process_id` - Target process ID

**Returns:** `BatchReadResult` with statistics

**Example:**
```cpp
#include <VolkDMA/batch.hh>

// Prepare requests
std::vector<VolkDMA::ReadRequest> requests;
requests.emplace_back(0x1000, 4);  // Read 4 bytes at 0x1000
requests.emplace_back(0x2000, 8);  // Read 8 bytes at 0x2000

// Execute batch read
auto result = dma.batch_read(requests, pid);

// Check results
for (size_t i = 0; i < requests.size(); ++i) {
    if (requests[i].success) {
        std::cout << "Read " << requests[i].bytes_read << " bytes\n";
    }
}
```

**Performance:** 50-80% faster than individual reads

---

### **enable_metrics()**

```cpp
void enable_metrics(bool enable);
```

**Description:** Enables/disables performance metrics collection (v1.2).

**Parameters:**
- `enable` - `true` to enable, `false` to disable

**Example:**
```cpp
dma.enable_metrics(true);  // Start tracking performance
// ... perform operations ...
dma.log_metrics_summary(); // Print metrics
```

**Performance:** Zero overhead when disabled

---

### **get_metrics()**

```cpp
DMAMetrics get_metrics() const;
```

**Description:** Returns current performance metrics.

**Returns:** `DMAMetrics` struct with statistics

**Example:**
```cpp
auto metrics = dma.get_metrics();
std::cout << "Total reads: " << metrics.total_reads << "\n";
std::cout << "Success rate: " << metrics.get_success_rate() * 100 << "%\n";
```

---

### **dump_memory_region()** (v1.9)

```cpp
void dump_memory_region(uint64_t address, size_t size,
                       const std::string& filename,
                       DWORD process_id,
                       DumpFormat format = DumpFormat::Binary);
```

**Description:** Exports memory region to file.

**Parameters:**
- `address` - Start address
- `size` - Number of bytes
- `filename` - Output file path
- `process_id` - Target process ID
- `format` - Dump format (Binary, HexDump, CArray, IDA)

**Example:**
```cpp
// Binary dump for IDA Pro
dma.dump_memory_region(
    0x7FF700000000,
    1024 * 1024,  // 1 MB
    "memory.bin",
    pid,
    VolkDMA::DumpFormat::Binary
);

// Hex dump for viewing
dma.dump_memory_region(
    0x7FF700000000,
    256,
    "memory.txt",
    pid,
    VolkDMA::DumpFormat::HexDump
);
```

---

## 🔷 **Validation** (v1.1)

Input validation utilities in `<VolkDMA/validators.hh>`.

### **SignatureValidator**

```cpp
namespace VolkDMA::Validation {
    class SignatureValidator {
        static bool is_valid_hex_pattern(const std::string& pattern);
        static size_t get_pattern_length(const std::string& pattern);
    };
}
```

**Example:**
```cpp
if (!SignatureValidator::is_valid_hex_pattern("48 8B 05 ? ? ? ?")) {
    std::cerr << "Invalid pattern format!\n";
}
```

### **MemoryRangeValidator**

```cpp
class MemoryRangeValidator {
    static bool is_safe_range(uint64_t start, uint64_t end, size_t size);
    static bool would_overflow(uint64_t start, size_t size);
};
```

**Example:**
```cpp
if (!MemoryRangeValidator::is_safe_range(start, end, size)) {
    std::cerr << "Invalid memory range!\n";
}
```

---

## 🔷 **Metrics** (v1.2)

Performance tracking in `<VolkDMA/metrics.hh>`.

### **DMAMetrics**

```cpp
struct DMAMetrics {
    uint64_t total_reads;
    uint64_t successful_reads;
    uint64_t failed_reads;
    
    double total_read_time_ms;
    double average_read_time_ms;
    
    double get_success_rate() const;
    double get_throughput_mbps() const;
};
```

**Example:**
```cpp
auto metrics = dma.get_metrics();
std::cout << "Success rate: " << metrics.get_success_rate() * 100 << "%\n";
std::cout << "Avg read time: " << metrics.average_read_time_ms << " ms\n";
```

---

## 🔷 **Configuration** (v1.4)

Runtime configuration in `<VolkDMA/config.hh>`.

### **DMAConfiguration**

```cpp
class DMAConfiguration {
    static DMAConfiguration& instance();
    
    void load_from_file(const std::string& filename);
    void save_to_file(const std::string& filename);
    
    bool get_bool(const std::string& key) const;
    int get_int(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
};
```

**Example:**
```cpp
auto& config = DMAConfiguration::instance();
config.load_from_file("volkdma.ini");

bool use_cache = config.get_bool("memory.enable_cache");
int chunk_size = config.get_int("scanning.chunk_size");
```

---

## 🔷 **Memory Cache** (v1.5)

Caching system in `<VolkDMA/cache.hh>`.

### **MemoryCache**

```cpp
class MemoryCache {
    void set_enabled(bool enabled);
    void clear();
    void invalidate(uint64_t address);
    
    CacheStatistics get_statistics() const;
};
```

**Example:**
```cpp
auto& cache = dma.get_cache();
cache.set_enabled(true);  // Enable caching

// ... perform reads (cached automatically) ...

auto stats = cache.get_statistics();
std::cout << "Cache hit rate: " << stats.get_hit_rate() * 100 << "%\n";
```

**Performance:** 10-100x speedup for repeated reads

---

## 🔷 **Memory Layout** (v1.6)

Memory analysis in `<VolkDMA/memory_layout.hh>`.

### **MemoryRegion**

```cpp
struct MemoryRegion {
    uint64_t base_address;
    size_t size;
    Protection protection;
    MemoryType type;
    MemoryState state;
    std::string module_name;
    
    bool is_executable() const;
    bool is_readable() const;
    uint64_t end_address() const;
};
```

### **MemoryLayoutAnalyzer**

```cpp
class MemoryLayoutAnalyzer {
    std::vector<MemoryRegion> get_memory_layout(DWORD process_id);
    std::vector<MemoryRegion> get_executable_regions(DWORD process_id);
    std::optional<MemoryRegion> find_module(const std::string& name, DWORD process_id);
};
```

**Example:**
```cpp
MemoryLayoutAnalyzer analyzer;

// Get all memory regions
auto regions = analyzer.get_memory_layout(pid);

// Find specific module
auto game_dll = analyzer.find_module("game.dll", pid);
if (game_dll.has_value()) {
    std::cout << "Module base: 0x" << std::hex << game_dll->base_address << "\n";
    std::cout << "Module size: " << game_dll->size << " bytes\n";
}
```

---

## 🔷 **Batch Operations** (v1.7)

Batch reading in `<VolkDMA/batch.hh>`.

### **ReadRequest**

```cpp
struct ReadRequest {
    uint64_t address;
    size_t size;
    std::vector<uint8_t> data;  // Output
    
    bool success;
    size_t bytes_read;
    std::string error_message;
};
```

### **BatchReadResult**

```cpp
struct BatchReadResult {
    size_t successful_reads;
    size_t failed_reads;
    size_t total_bytes_read;
    
    double duration_ms;
    double throughput_mbps;
    
    bool all_succeeded() const;
    double get_success_rate() const;
};
```

**Example:**
```cpp
std::vector<ReadRequest> requests;
requests.emplace_back(0x1000, 4);
requests.emplace_back(0x2000, 8);

auto result = dma.batch_read(requests, pid);
std::cout << "Success rate: " << result.get_success_rate() * 100 << "%\n";
```

---

## 🔷 **Health Monitoring** (v1.8)

System health in `<VolkDMA/health.hh>`.

### **HealthMonitor**

```cpp
class HealthMonitor {
    HealthCheck check_fpga_connection();
    HealthCheck check_performance();
    HealthStatus get_overall_status();
    
    void start_monitoring(std::chrono::seconds interval);
    void stop_monitoring();
};
```

**Example:**
```cpp
dma.start_automatic_health_monitoring(std::chrono::seconds(30));

// Later...
auto status = dma.get_health_status();
if (status == HealthStatus::Critical) {
    std::cerr << "System health critical!\n";
}
```

---

## 🔷 **Memory Dumper** (v1.9)

Memory export in `<VolkDMA/dumper.hh>`.

### **DumpFormat**

```cpp
enum class DumpFormat {
    Binary,   // Raw binary
    HexDump,  // Human-readable hex
    CArray,   // C array format
    IDA       // IDA Pro .idc script
};
```

### **MemoryDumper**

```cpp
class MemoryDumper {
    void dump_region(uint64_t address, size_t size, 
                    const std::string& filename,
                    DWORD process_id,
                    DumpFormat format);
                    
    void dump_module(const std::string& module_name,
                    const std::string& filename,
                    DWORD process_id,
                    DumpFormat format);
                    
    std::vector<uint64_t> compare_dumps(const std::string& file1,
                                       const std::string& file2);
};
```

**Example:**
```cpp
MemoryDumper dumper;

// Dump entire module
dumper.dump_module("client.dll", "client_dump.bin", pid, DumpFormat::Binary);

// Compare two dumps
auto changes = dumper.compare_dumps("before.bin", "after.bin");
std::cout << "Changed addresses: " << changes.size() << "\n";
```

---

## 📖 **Quick Reference**

### **Most Common Operations**

```cpp
// Initialize
VolkDMA::DMA dma;
dma.initialize();

// Find process
DWORD pid = dma.get_process_id("game.exe");

// Read memory
uint32_t value = dma.read<uint32_t>(address, pid);

// Scan signature
uint64_t addr = dma.find_signature("48 8B 05 ? ? ? ?", start, end, pid);

// Batch read
std::vector<ReadRequest> requests = {...};
auto result = dma.batch_read(requests, pid);

// Dump memory
dma.dump_memory_region(address, size, "dump.bin", pid);
```

### **Performance Features**

```cpp
// Enable caching (10-100x speedup)
auto& cache = dma.get_cache();
cache.set_enabled(true);

// Smart scanning (80-90% faster)
uint64_t addr = dma.find_signature_in_module("pattern", "game.dll", pid);

// Batch operations (50-80% less overhead)
auto result = dma.batch_read(requests, pid);

// Health monitoring
dma.start_automatic_health_monitoring(std::chrono::seconds(30));
```

---

## 📚 **See Also**

- [GETTING_STARTED.md](GETTING_STARTED.md) - Tutorial for beginners
- [EXAMPLES.md](EXAMPLES.md) - Copy-paste code examples
- [FAQ.md](FAQ.md) - Common questions and troubleshooting
- [PERFORMANCE.md](PERFORMANCE.md) - Optimization guide

---

**Need more help?** Check [FAQ.md](FAQ.md) or open an issue on [GitHub](https://github.com/YourUsername/VolkDMA/issues)
