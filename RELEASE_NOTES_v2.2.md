# 🏗️ VolkDMA v2.2 - Fluent Builder Interface

**Release Date:** 11 Martie 2026  
**Build Status:** ✅ STABLE  
**Library Size:** 39.2 MB  
**Breaking Changes:** ❌ None (100% backward compatible)

---

## 📋 **What's New in v2.2**

### ✨ **Major Feature: Fluent Builder Pattern**

v2.2 introduces an elegant, type-safe configuration API using the **Fluent Builder pattern**. Configure DMA instances with intuitive method chaining instead of multiple setter calls.

**Example:**
```cpp
#include "VolkDMA/dma.hh"

// Before (v2.1):
DMA dma(true);  // Constructor with memory map
dma.enable_cache(true);
dma.set_cache_size(100 * 1024 * 1024);
dma.set_cache_ttl(std::chrono::seconds(60));
dma.enable_metrics(true);
dma.enable_health_monitoring(true);
dma.start_automatic_health_monitoring();

// After (v2.2):
auto dma = DMA::Builder()
    .with_cache(100 * 1024 * 1024, std::chrono::seconds(60))
    .with_metrics(true)
    .with_health_monitoring(true, true)  // Enable + auto-start
    .build();
```

---

## 🎯 **Key Features**

### 1. **DMABuilder Class**
Complete fluent configuration interface with 8 configuration methods:

```cpp
DMABuilder& with_memory_map(bool enable);
DMABuilder& with_fpga_algorithm(int algo);
DMABuilder& with_cache(size_t size, std::chrono::seconds ttl);
DMABuilder& with_metrics(bool enable);
DMABuilder& with_health_monitoring(bool enable, bool auto_start = false);
DMABuilder& with_logging(int level);
DMABuilder& with_scan_chunk_size(size_t chunk_size);
DMABuilder& with_max_read_size(size_t max_size);
std::unique_ptr<DMA> build() const;
```

### 2. **Three Static Presets**
Quick configuration for common use cases:

```cpp
// Production preset: 100MB cache, 60s TTL, metrics+health enabled
auto dma = DMA::Builder::production().build();

// Development preset: 10MB cache, 5s TTL, debug logging
auto dma = DMA::Builder::development().build();

// Testing preset: No cache, no metrics, minimal overhead
auto dma = DMA::Builder::testing().build();
```

### 3. **Validation System**
Runtime configuration validation with descriptive error messages:

```cpp
auto builder = DMA::Builder()
    .with_cache(5000 * 1024 * 1024, std::chrono::seconds(5));  // Invalid!

if (!builder.is_valid()) {
    std::cout << "Error: " << builder.get_validation_error() << "\n";
    // Output: "Cache size exceeds maximum (1GB)"
}
```

**Validation Rules:**
- Cache size: 1 KB - 1 GB
- Cache TTL: 1s - 1 hour
- Scan chunk size: 4 KB - 100 MB
- Max read size: 1 KB - 100 MB

### 4. **Dynamic Cache Reconfiguration**
New runtime configuration methods for cache:

```cpp
auto dma = DMA::Builder::production().build();

// Adjust cache at runtime
dma->set_cache_size(50 * 1024 * 1024);  // Reduce to 50MB
dma->set_cache_ttl(std::chrono::seconds(30));  // Change TTL to 30s

// Query current settings
size_t current_size = dma->get_cache_max_size();
auto current_ttl = dma->get_cache_ttl();
```

---

## 📊 **Technical Specifications**

### **Files Added/Modified:**

**New Files:**
- `include/VolkDMA/builder.hh` (~180 lines) - Builder pattern header
- `src/builder.cpp` (~260 lines) - Complete implementation

**Enhanced Files:**
- `include/VolkDMA/dma.hh` - Added Builder() factory, cache config methods
- `src/dma.cpp` - Implemented Builder() and cache configuration
- `include/VolkDMA/cache.hh` - Added dynamic reconfiguration methods
- `src/cache.cpp` - Implemented set_max_size(), set_ttl() with thread-safety
- `VolkDMA.vcxproj` - Added builder.cpp/builder.hh to project

**Total Code:** ~440 lines of production code

### **Build Results:**
```
✅ VolkDMARelease.lib - 39.2 MB (Release x64)
✅ TestDMA.exe - 473 KB (functional, all tests pass)
✅ Zero compilation errors
✅ Only benign warnings (external headers)
```

### **Compilation Challenges Overcome:**
1. **DMA non-copyable** - Changed `build()` to return `std::unique_ptr<DMA>` instead of DMA by value
2. **MetricsCollector API** - Fixed to use `reset_metrics()` instead of `reset()`
3. **Cache reconfiguration** - Implemented thread-safe set_max_size() and set_ttl()
4. **2 compilation iterations** - Clean build achieved

---

## 🚀 **Performance Impact**

**Compilation:**
- Library size: +1 MB (v2.1: 38.1 MB → v2.2: 39.2 MB)
- No runtime overhead (configuration happens at build time)

**Runtime:**
- Zero impact - Builder pattern only used during initialization
- Same performance as manual configuration (v2.1)
- All v2.1 optimizations still active (cache, async, batch, etc.)

---

## 📖 **Usage Examples**

### **Example 1: Production Configuration**
```cpp
#include "VolkDMA/dma.hh"
#include <iostream>

int main() {
    // Production-ready configuration
    auto dma = DMA::Builder()
        .with_cache(100 * 1024 * 1024, std::chrono::seconds(60))
        .with_metrics(true)
        .with_health_monitoring(true, true)  // Enable + auto-start
        .with_logging(2)  // Warning level
        .build();

    // Use DMA normally
    DWORD pid = dma->get_process_id("game.exe");
    auto health = dma->read<int32_t>(0x12345678, pid);

    std::cout << "Health: " << health << "\n";
    return 0;
}
```

### **Example 2: Quick Preset**
```cpp
// Use production preset (one-liner)
auto dma = DMA::Builder::production().build();

// Or development preset for testing
auto dma = DMA::Builder::development().build();

// Or testing preset (no overhead)
auto dma = DMA::Builder::testing().build();
```

### **Example 3: Custom Configuration**
```cpp
auto dma = DMA::Builder()
    .with_memory_map(false)          // Direct FPGA access
    .with_fpga_algorithm(1)          // Alternative algorithm
    .with_cache(50 * 1024 * 1024, std::chrono::seconds(30))
    .with_logging(3)                 // Debug level
    .with_scan_chunk_size(2 * 1024 * 1024)  // 2MB chunks
    .build();
```

### **Example 4: Runtime Reconfiguration**
```cpp
auto dma = DMA::Builder::production().build();

// Later in code, adjust cache based on memory pressure
if (system_memory_low()) {
    dma->set_cache_size(10 * 1024 * 1024);  // Reduce to 10MB
}

// Adjust TTL based on game update frequency
if (game_updates_fast()) {
    dma->set_cache_ttl(std::chrono::seconds(5));  // Reduce TTL
}
```

---

## ✅ **Backward Compatibility**

**100% backward compatible with v2.1:**
- All existing code continues to work without changes
- Traditional constructor still available: `DMA dma(true);`
- All setter methods still functional
- Builder pattern is **optional** - use it when convenient

**Migration Path:**
- No migration required - existing code runs unchanged
- Adopt Builder pattern gradually as you refactor
- Use presets for new projects to save time

---

## 🎓 **Design Patterns Used**

1. **Fluent Interface** - Method chaining for readable configuration
2. **Builder Pattern** - Separate construction from representation
3. **Factory Method** - Static `DMA::Builder()` creates builder instance
4. **Named Constructor Idiom** - Static presets (production, development, testing)
5. **RAII** - Automatic resource management via `unique_ptr<DMA>`

---

## 📦 **Dependencies**

**No new dependencies added:**
- Same dependencies as v2.1
- C++17 standard library
- MemProcFS (vmmdll.lib, leechcore.lib)
- No external libraries for builder pattern

---

## 🔧 **Upgrade Guide**

### **From v2.1 to v2.2:**

**Option 1: No changes required**
```cpp
// Your existing v2.1 code works unchanged
DMA dma(true);
dma.enable_cache(true);
dma.enable_metrics(true);
// ... works exactly as before
```

**Option 2: Adopt Builder gradually**
```cpp
// Refactor when convenient
auto dma = DMA::Builder()
    .with_cache(100 * 1024 * 1024)
    .with_metrics(true)
    .build();
```

**Option 3: Use presets for new code**
```cpp
// Fastest way to get started
auto dma = DMA::Builder::production().build();
```

---

## 🐛 **Known Issues**

**None** - v2.2 is stable and production-ready.

---

## 📝 **Coming Next**

**Planned for v2.3+:**
1. **Rate Limiting** (~3-4h) - Anti-detection, system stability
2. **Mock Interface** (~4-5h) - Testing without hardware
3. **Pattern Compilation** (~4-5h) - Pre-compiled patterns for 2-3x speedup

See `ROADMAP.md` for complete feature list and priorities.

---

## 🙏 **Credits**

**VolkDMA v2.2** - Fluent Builder Interface  
**Developer:** [Your Name]  
**Date:** 11 Martie 2026  
**License:** [Your License]

**Special Thanks:**
- MemProcFS team for FPGA DMA framework
- C++ community for design pattern inspiration
- All contributors and testers

---

## 📚 **Resources**

- **Documentation:** See `ROADMAP.md` for complete feature documentation
- **Examples:** Check `example/` directory for sample code
- **Testing:** Run `TestDMA.exe` to verify installation
- **Support:** [Your support channel]

---

## 🔗 **Version History**

- **v2.2** (11 Mar 2026) - Fluent Builder Interface ✅
- **v2.1** (11 Mar 2026) - Memory Diffing (Cheat Engine-style) 🔍
- **v2.0** (11 Mar 2026) - Async Operations (2-4x speedup) 🚀
- **v1.9** (10 Mar 2026) - Memory Dump Utilities (4 formats) 📦
- **v1.8** (10 Mar 2026) - Health Monitoring (FPGA status) 🏥
- **v1.7** (10 Mar 2026) - Batch Read Operations (50-80% reduction) 💪
- **v1.6** (10 Mar 2026) - Memory Layout Analysis (80-90% speedup) 🚀
- **v1.5** (10 Mar 2026) - Memory Cache System (10-100x speedup) ⚡
- **v1.0-v1.4** - Foundation, Validation, Metrics, Testing, Configuration

**VolkDMA is production-ready! 🎉**
