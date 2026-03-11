# ⚙️ VolkDMA Configuration System

## Overview

Configuration System permite customizarea runtime a VolkDMA fără recompilare. Toate settings pot fi modificate prin fișiere INI sau programatic.

## Features

✅ **INI File Support** - Standard INI format cu comments  
✅ **Runtime Configuration** - Change settings fără rebuild  
✅ **Type-Safe API** - Getters/setters cu validare  
✅ **Singleton Pattern** - Global access la configurație  
✅ **Save/Load** - Persistent configuration  
✅ **Reset to Defaults** - Easy recovery  
✅ **Thread-Safe** - Singleton cu lazy initialization  

---

## Quick Start

### 1. Load Configuration

```cpp
#include "include/VolkDMA/config.hh"

auto& config = VolkDMA::Config::GlobalConfig();
config.load_from_file("volkdma.ini");
```

### 2. Modify Settings

```cpp
// Increase scan chunk size
config.set_scan_chunk_size(2 * 1024 * 1024); // 2MB

// Disable metrics for speed
config.set_metrics_enabled(false);
```

### 3. Save Configuration

```cpp
config.save_to_file("volkdma_custom.ini");
```

---

## Configuration File Format

### Example: `volkdma.ini`

```ini
; VolkDMA Configuration File

[FPGA]
algorithm = 0
min_version_major = 4
min_version_minor = 7

[Scanning]
chunk_size = 1048576  ; 1MB chunks

[Memory]
max_safe_read_size = 2147483648  ; 2GB max

[Metrics]
enabled = true

[Logging]
level = info  ; debug, info, warn, error
```

---

## Settings Reference

### FPGA Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `algorithm` | int | 0 | FPGA algorithm to use |
| `min_version_major` | uint64 | 4 | Minimum FPGA version (major) |
| `min_version_major_alt` | uint64 | 5 | Alternative major version |
| `min_version_minor` | uint64 | 7 | Minimum FPGA version (minor) |

### Scanning Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `chunk_size` | size_t | 1048576 (1MB) | Signature scan chunk size |

**Performance Impact:**
- Larger chunks = faster scanning, more memory
- Smaller chunks = slower scanning, less memory
- Recommended: 512KB - 4MB

### Memory Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `max_safe_read_size` | size_t | 2147483648 (2GB) | Maximum safe read size |

**Safety:**
- Protects against overflow
- Prevents excessive memory allocation
- Recommended: 1GB - 4GB

### Metrics Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | bool | true | Enable performance metrics |

**Values:** `true`, `false`, `yes`, `no`, `on`, `off`, `1`, `0`

### Logging Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `level` | string | "info" | Logging verbosity level |

**Levels:**
- `debug` - All messages (verbose)
- `info` - Informational and higher
- `warn` - Warnings and errors only
- `error` - Errors only

---

## API Reference

### Loading & Saving

```cpp
// Load configuration from file
bool load_from_file(const std::string& filepath);

// Save current configuration to file
bool save_to_file(const std::string& filepath) const;

// Reset all settings to defaults
void reset_to_defaults();
```

### FPGA Settings

```cpp
int get_fpga_algorithm() const;
void set_fpga_algorithm(int value);

uint64_t get_fpga_min_version_major() const;
void set_fpga_min_version_major(uint64_t value);

uint64_t get_fpga_min_version_minor() const;
void set_fpga_min_version_minor(uint64_t value);
```

### Scanning Settings

```cpp
size_t get_scan_chunk_size() const;
void set_scan_chunk_size(size_t value);
```

### Memory Settings

```cpp
size_t get_max_safe_read_size() const;
void set_max_safe_read_size(size_t value);
```

### Metrics Settings

```cpp
bool is_metrics_enabled() const;
void set_metrics_enabled(bool value);
```

### Logging Settings

```cpp
const std::string& get_log_level() const;
void set_log_level(const std::string& level);
```

---

## Usage Examples

### Example 1: Performance Tuning

```cpp
auto& config = VolkDMA::Config::GlobalConfig();

// Large game memory - optimize for speed
config.set_scan_chunk_size(4 * 1024 * 1024); // 4MB chunks
config.set_max_safe_read_size(4ULL * 1024 * 1024 * 1024); // 4GB
config.save_to_file("volkdma_fast.ini");
```

### Example 2: Memory-Constrained System

```cpp
auto& config = VolkDMA::Config::GlobalConfig();

// Limited RAM - optimize for low memory
config.set_scan_chunk_size(256 * 1024); // 256KB chunks
config.set_max_safe_read_size(512 * 1024 * 1024); // 512MB
config.set_metrics_enabled(false); // Save memory
config.save_to_file("volkdma_lowmem.ini");
```

### Example 3: Development Mode

```cpp
auto& config = VolkDMA::Config::GlobalConfig();

// Development - verbose logging and metrics
config.set_metrics_enabled(true);
config.set_log_level("debug");
config.save_to_file("volkdma_debug.ini");
```

### Example 4: Production Deployment

```cpp
auto& config = VolkDMA::Config::GlobalConfig();

// Production - balanced performance
config.set_scan_chunk_size(2 * 1024 * 1024); // 2MB
config.set_metrics_enabled(false); // Maximum speed
config.set_log_level("error"); // Errors only
config.save_to_file("volkdma_production.ini");
```

---

## Testing

Run configuration system tests:

```bash
cd tests
g++ -std=c++17 config_test.cpp ../src/config.cpp -I.. -o config_test
./config_test
```

Expected output:

```
╔════════════════════════════════════════════════════════════╗
║         VolkDMA Configuration System Tests                 ║
╚════════════════════════════════════════════════════════════╝

=== TEST: Default Values ===
✓ All default values correct

=== TEST: Setters and Getters ===
✓ FPGA algorithm setter/getter works
✓ Scan chunk size setter/getter works
... (more tests)

╔════════════════════════════════════════════════════════════╗
║                  ALL TESTS PASSED! ✓                       ║
╚════════════════════════════════════════════════════════════╝
```

---

## Best Practices

### 1. **Load Configuration Early**
```cpp
int main() {
    // Load config before creating DMA instance
    VolkDMA::Config::GlobalConfig().load_from_file("volkdma.ini");
    
    DMA dma; // Uses loaded configuration
    // ...
}
```

### 2. **Profile Different Configurations**
```cpp
// Test different chunk sizes
for (size_t chunk_size : {512*1024, 1024*1024, 2*1024*1024, 4*1024*1024}) {
    auto& config = GlobalConfig();
    config.set_scan_chunk_size(chunk_size);
    
    // Benchmark scanning...
    
    config.save_to_file("volkdma_" + std::to_string(chunk_size) + ".ini");
}
```

### 3. **Ship Multiple Configs**
```
volkdma_default.ini   - Balanced settings
volkdma_fast.ini      - Maximum speed
volkdma_lowmem.ini    - Low memory usage
volkdma_debug.ini     - Development mode
```

### 4. **Validate Before Deployment**
```cpp
auto& config = GlobalConfig();
config.load_from_file("volkdma.ini");

// Sanity checks
if (config.get_scan_chunk_size() < 4096) {
    std::cerr << "Warning: Chunk size too small!" << std::endl;
}

if (config.get_max_safe_read_size() > 8ULL * 1024 * 1024 * 1024) {
    std::cerr << "Warning: Read size exceeds 8GB!" << std::endl;
}
```

---

## Troubleshooting

### Configuration Not Loading

**Problem:** `load_from_file()` returns false

**Solutions:**
1. Check file exists and is readable
2. Verify file path is correct
3. Check file format (valid INI syntax)
4. Look for error messages in console

### Settings Not Taking Effect

**Problem:** Changed settings but behavior unchanged

**Solutions:**
1. Ensure configuration loaded **before** DMA instance creation
2. Check if setting is cached by DMA instance
3. Verify setting is actually being used (check code integration)

### Invalid Values

**Problem:** Settings with invalid values

**Solutions:**
1. Check type matches (int, uint64, bool, string)
2. Verify boolean format: true/false, yes/no, on/off, 1/0
3. Check numeric values are within valid range
4. INI parser skips invalid lines silently - check for errors

---

## Implementation Details

### Thread Safety

- **Singleton instance** is thread-safe (lazy initialization)
- **Individual settings** are NOT thread-safe (no mutex on getters/setters)
- **Recommendation:** Load configuration during initialization (single-threaded)

### Memory Usage

- Configuration object: ~200 bytes
- No dynamic allocation for settings
- INI parsing uses std::string (temporary allocations)

### Performance

- Load/save: ~1-5ms for typical config file
- Getters: ~1-5 CPU cycles (inline)
- Setters: ~5-10 CPU cycles
- Zero overhead when not actively loading/saving

---

## See Also

- [USAGE_EXAMPLES.md](../USAGE_EXAMPLES.md) - More usage examples
- [ROADMAP.md](../ROADMAP.md) - Future configuration features
- [volkdma.ini](../volkdma.ini) - Default configuration file
