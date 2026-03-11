# 💡 VolkDMA Examples

Copy-paste ready code for common use cases. All examples are complete and compilable.

---

## 📋 **Table of Contents**

1. [Basic Memory Reading](#1-basic-memory-reading)
2. [Finding Signature Patterns](#2-finding-signature-patterns)
3. [Using Memory Cache](#3-using-memory-cache)
4. [Batch Reading (ESP/Radar)](#4-batch-reading-espradar)
5. [Dumping Memory](#5-dumping-memory)
6. [Health Monitoring Setup](#6-health-monitoring-setup)
7. [Smart Scanning](#7-smart-scanning)
8. [Memory Comparison](#8-memory-comparison)

---

## 1. **Basic Memory Reading**

**Problem:** Read health, armor, and ammo from a game.

```cpp
#include <iostream>
#include <VolkDMA/dma.hh>

int main() {
    VolkDMA::DMA dma;
    
    // Initialize
    if (!dma.initialize()) {
        std::cerr << "❌ Failed to initialize DMA\n";
        return 1;
    }
    
    // Find process
    DWORD pid = dma.get_process_id("game.exe");
    if (pid == 0) {
        std::cerr << "❌ Game not found\n";
        return 1;
    }
    
    std::cout << "✅ Found game (PID: " << pid << ")\n\n";
    
    // Read player stats (replace with actual addresses)
    uint64_t player_base = 0x7FF700123456;
    
    try {
        int health = dma.read<int>(player_base + 0x100, pid);
        int armor  = dma.read<int>(player_base + 0x104, pid);
        int ammo   = dma.read<int>(player_base + 0x108, pid);
        
        std::cout << "Player Stats:\n";
        std::cout << "  Health: " << health << "\n";
        std::cout << "  Armor:  " << armor << "\n";
        std::cout << "  Ammo:   " << ammo << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Read failed: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

**Expected Output:**
```
✅ Found game (PID: 12345)

Player Stats:
  Health: 100
  Armor:  50
  Ammo:   30
```

**Common Pitfalls:**
- ⚠️ Replace `0x7FF700123456` with actual address from your analysis
- ⚠️ Offsets (`+0x100`, `+0x104`) must be correct for your game
- ⚠️ Values will be garbage if addresses are wrong

---

## 2. **Finding Signature Patterns**

**Problem:** Find entity list pointer using signature scan.

```cpp
#include <iostream>
#include <iomanip>
#include <VolkDMA/dma.hh>

int main() {
    VolkDMA::DMA dma;
    
    if (!dma.initialize()) {
        std::cerr << "❌ DMA init failed\n";
        return 1;
    }
    
    DWORD pid = dma.get_process_id("game.exe");
    if (pid == 0) {
        std::cerr << "❌ Game not found\n";
        return 1;
    }
    
    std::cout << "🔍 Scanning for entity list pattern...\n";
    
    // Pattern: MOV RCX, [RIP+offset]
    const char* pattern = "48 8B 0D ? ? ? ? 48 85 C9";
    
    // Scan range (typically .text section)
    uint64_t range_start = 0x7FF700000000;
    uint64_t range_end   = 0x7FF702000000; // 32 MB
    
    try {
        uint64_t pattern_addr = dma.find_signature(pattern, range_start, range_end, pid);
        
        if (pattern_addr == 0) {
            std::cout << "❌ Pattern not found\n";
            return 1;
        }
        
        std::cout << "✅ Pattern found at: 0x" << std::hex << pattern_addr << "\n";
        
        // Read the relative offset (at pattern +3 bytes)
        int32_t relative_offset = dma.read<int32_t>(pattern_addr + 3, pid);
        
        // Calculate absolute address
        // RIP-relative: RIP + offset + instruction_length
        uint64_t entity_list_ptr_addr = pattern_addr + 7 + relative_offset;
        
        std::cout << "✅ Entity list pointer at: 0x" << std::hex << entity_list_ptr_addr << "\n";
        
        // Dereference to get actual entity list
        uint64_t entity_list = dma.read<uint64_t>(entity_list_ptr_addr, pid);
        std::cout << "✅ Entity list base: 0x" << std::hex << entity_list << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

**Expected Output:**
```
🔍 Scanning for entity list pattern...
✅ Pattern found at: 0x7ff700abc123
✅ Entity list pointer at: 0x7ff702def456
✅ Entity list base: 0x123456789abc
```

**Understanding RIP-Relative Addressing:**
```
Pattern: 48 8B 0D ? ? ? ?
         MOV RCX, [RIP+offset]

Address of instruction: 0x7FF700ABC123
Relative offset (at +3): 0x12345678
Instruction length: 7 bytes

Final address = 0x7FF700ABC123 + 7 + 0x12345678
              = RIP + instruction_len + offset
```

---

## 3. **Using Memory Cache**

**Problem:** Read same addresses repeatedly (e.g., entity positions every frame).

```cpp
#include <iostream>
#include <chrono>
#include <thread>
#include <VolkDMA/dma.hh>

int main() {
    VolkDMA::DMA dma;
    dma.initialize();
    
    DWORD pid = dma.get_process_id("game.exe");
    
    // Enable caching for 10-100x speedup
    auto& cache = dma.get_cache();
    cache.set_enabled(true);
    
    uint64_t health_addr = 0x7FF700123456;
    
    std::cout << "📊 Reading health 1000 times...\n";
    
    // WITHOUT CACHE
    cache.set_enabled(false);
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        int health = dma.read<int>(health_addr, pid);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_no_cache = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "⏱️  Without cache: " << duration_no_cache.count() << " ms\n";
    
    // WITH CACHE
    cache.set_enabled(true);
    cache.clear(); // Clear for fair comparison
    
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        int health = dma.read<int>(health_addr, pid);
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto duration_with_cache = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "⏱️  With cache: " << duration_with_cache.count() << " ms\n";
    
    // Calculate speedup
    double speedup = (double)duration_no_cache.count() / duration_with_cache.count();
    std::cout << "🚀 Speedup: " << std::fixed << std::setprecision(1) << speedup << "x faster\n";
    
    // Cache statistics
    auto stats = cache.get_statistics();
    std::cout << "\n📈 Cache Stats:\n";
    std::cout << "  Hits: " << stats.hits << "\n";
    std::cout << "  Misses: " << stats.misses << "\n";
    std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) 
              << (stats.get_hit_rate() * 100) << "%\n";
    
    return 0;
}
```

**Expected Output:**
```
📊 Reading health 1000 times...
⏱️  Without cache: 1245 ms
⏱️  With cache: 12 ms
🚀 Speedup: 103.8x faster

📈 Cache Stats:
  Hits: 999
  Misses: 1
  Hit rate: 99.9%
```

**When to Use Cache:**
- ✅ Reading same addresses repeatedly (health, position, etc.)
- ✅ Polling values every frame
- ❌ One-time reads (no benefit)
- ⚠️ Clear cache if memory changes externally

---

## 4. **Batch Reading (ESP/Radar)**

**Problem:** Read positions of 50 entities for ESP overlay.

```cpp
#include <iostream>
#include <VolkDMA/dma.hh>
#include <VolkDMA/batch.hh>

struct Vec3 {
    float x, y, z;
};

int main() {
    VolkDMA::DMA dma;
    dma.initialize();
    
    DWORD pid = dma.get_process_id("game.exe");
    
    // Entity list (replace with actual addresses)
    uint64_t entity_list = 0x7FF700123456;
    int entity_count = 50;
    size_t entity_size = 0x400; // Distance between entities
    
    // Prepare batch read requests
    std::vector<VolkDMA::ReadRequest> requests;
    
    for (int i = 0; i < entity_count; ++i) {
        uint64_t entity_addr = entity_list + (i * entity_size);
        uint64_t position_addr = entity_addr + 0x100; // Position offset
        
        requests.emplace_back(position_addr, sizeof(Vec3));
    }
    
    std::cout << "📊 Batch reading " << entity_count << " entity positions...\n";
    
    // Execute batch read (50-80% faster than individual reads)
    auto result = dma.batch_read(requests, pid);
    
    std::cout << "✅ Batch read complete!\n";
    std::cout << "  Success: " << result.successful_reads << "\n";
    std::cout << "  Failed: " << result.failed_reads << "\n";
    std::cout << "  Duration: " << std::fixed << std::setprecision(2) 
              << result.duration_ms << " ms\n";
    std::cout << "  Throughput: " << result.throughput_mbps << " MB/s\n";
    
    // Process results
    std::cout << "\n📍 Entity Positions:\n";
    for (size_t i = 0; i < requests.size(); ++i) {
        if (requests[i].success) {
            Vec3 pos;
            std::memcpy(&pos, requests[i].data.data(), sizeof(Vec3));
            
            std::cout << "  Entity " << i << ": ("
                      << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
        }
    }
    
    return 0;
}
```

**Expected Output:**
```
📊 Batch reading 50 entity positions...
✅ Batch read complete!
  Success: 50
  Failed: 0
  Duration: 12.45 ms
  Throughput: 45.2 MB/s

📍 Entity Positions:
  Entity 0: (100.5, 200.3, 300.1)
  Entity 1: (150.2, 180.7, 320.4)
  ...
```

**Performance Comparison:**
```
Individual reads: 50 reads × 1ms = 50ms
Batch read: 1 batch = 12ms
Speedup: 4.2x faster 🚀
```

---

## 5. **Dumping Memory**

**Problem:** Export module memory for analysis in IDA Pro.

```cpp
#include <iostream>
#include <VolkDMA/dma.hh>
#include <VolkDMA/dumper.hh>

int main() {
    VolkDMA::DMA dma;
    dma.initialize();
    
    DWORD pid = dma.get_process_id("game.exe");
    
    std::cout << "📦 Dumping game module...\n";
    
    // Dump entire module
    dma.dump_module(
        "client.dll",           // Module name
        "client_dump.bin",      // Output file
        pid,
        VolkDMA::DumpFormat::Binary
    );
    
    std::cout << "✅ Dumped to client_dump.bin\n";
    
    // Also create IDA Pro script
    dma.dump_module(
        "client.dll",
        "client_dump.idc",
        pid,
        VolkDMA::DumpFormat::IDA
    );
    
    std::cout << "✅ IDA script: client_dump.idc\n";
    std::cout << "\n💡 Open in IDA Pro:\n";
    std::cout << "  1. File > Load File > client_dump.bin\n";
    std::cout << "  2. File > Script File > client_dump.idc\n";
    
    // Create hex dump for viewing
    dma.dump_module(
        "client.dll",
        "client_dump.txt",
        pid,
        VolkDMA::DumpFormat::HexDump
    );
    
    std::cout << "✅ Hex dump: client_dump.txt (readable)\n";
    
    return 0;
}
```

**Expected Output:**
```
📦 Dumping game module...
✅ Dumped to client_dump.bin
✅ IDA script: client_dump.idc

💡 Open in IDA Pro:
  1. File > Load File > client_dump.bin
  2. File > Script File > client_dump.idc

✅ Hex dump: client_dump.txt (readable)
```

**Hex Dump Format (client_dump.txt):**
```
0x7FF700000000: 4D 5A 90 00 03 00 00 00 | MZ......
0x7FF700000008: 04 00 00 00 FF FF 00 00 | ........
0x7FF700000010: B8 00 00 00 00 00 00 00 | ........
```

---

## 6. **Health Monitoring Setup**

**Problem:** Monitor FPGA connection and automatically detect issues.

```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <VolkDMA/dma.hh>

int main() {
    VolkDMA::DMA dma;
    dma.initialize();
    
    // Start automatic health monitoring (every 30 seconds)
    dma.start_automatic_health_monitoring(std::chrono::seconds(30));
    
    std::cout << "🏥 Health monitoring started (checks every 30s)\n";
    std::cout << "Press Ctrl+C to stop...\n\n";
    
    // Simulate long-running application
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Check health status
        auto status = dma.get_health_status();
        
        std::cout << "Health check " << (i+1) << ": ";
        
        switch (status) {
            case VolkDMA::HealthStatus::Healthy:
                std::cout << "✅ HEALTHY\n";
                break;
            case VolkDMA::HealthStatus::Degraded:
                std::cout << "⚠️  DEGRADED\n";
                std::cout << dma.get_health_summary() << "\n";
                break;
            case VolkDMA::HealthStatus::Unhealthy:
                std::cout << "❌ UNHEALTHY\n";
                std::cout << dma.get_health_summary() << "\n";
                break;
            case VolkDMA::HealthStatus::Critical:
                std::cout << "🔴 CRITICAL\n";
                std::cout << dma.get_health_summary() << "\n";
                // Take action: restart, alert, etc.
                break;
        }
    }
    
    // Stop monitoring
    dma.stop_automatic_health_monitoring();
    std::cout << "\n🏥 Health monitoring stopped\n";
    
    return 0;
}
```

**Expected Output:**
```
🏥 Health monitoring started (checks every 30s)
Press Ctrl+C to stop...

Health check 1: ✅ HEALTHY
Health check 2: ✅ HEALTHY
Health check 3: ⚠️  DEGRADED
  - Performance: 35% slower than baseline
  - Recommendation: Check FPGA connection

Health check 4: ✅ HEALTHY
...
```

---

## 7. **Smart Scanning**

**Problem:** Find pattern in large module quickly.

```cpp
#include <iostream>
#include <chrono>
#include <VolkDMA/dma.hh>

int main() {
    VolkDMA::DMA dma;
    dma.initialize();
    
    DWORD pid = dma.get_process_id("game.exe");
    const char* pattern = "E8 ? ? ? ? 48 8B";
    
    std::cout << "🔍 Scanning for pattern...\n\n";
    
    // METHOD 1: Traditional full scan (slow)
    std::cout << "Method 1: Full memory scan\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    uint64_t addr1 = dma.find_signature(
        pattern,
        0x7FF700000000,
        0x7FF710000000, // 256 MB range
        pid
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Result: 0x" << std::hex << addr1 << "\n";
    std::cout << "  Time: " << std::dec << duration1.count() << " ms\n\n";
    
    // METHOD 2: Module-specific scan (fast)
    std::cout << "Method 2: Module-specific scan\n";
    start = std::chrono::high_resolution_clock::now();
    
    uint64_t addr2 = dma.find_signature_in_module(
        pattern,
        "client.dll",
        pid
    );
    
    end = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Result: 0x" << std::hex << addr2 << "\n";
    std::cout << "  Time: " << std::dec << duration2.count() << " ms\n\n";
    
    // METHOD 3: Executable-only scan (very fast)
    std::cout << "Method 3: Executable regions only\n";
    start = std::chrono::high_resolution_clock::now();
    
    uint64_t addr3 = dma.find_signature_in_executable(pattern, pid);
    
    end = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Result: 0x" << std::hex << addr3 << "\n";
    std::cout << "  Time: " << std::dec << duration3.count() << " ms\n\n";
    
    // Compare speedups
    std::cout << "📊 Speedup:\n";
    std::cout << "  Module scan: " << std::fixed << std::setprecision(1)
              << ((double)duration1.count() / duration2.count()) << "x faster\n";
    std::cout << "  Executable scan: " << ((double)duration1.count() / duration3.count()) 
              << "x faster\n";
    
    return 0;
}
```

**Expected Output:**
```
🔍 Scanning for pattern...

Method 1: Full memory scan
  Result: 0x7ff700abc123
  Time: 850 ms

Method 2: Module-specific scan
  Result: 0x7ff700abc123
  Time: 95 ms

Method 3: Executable regions only
  Result: 0x7ff700abc123
  Time: 120 ms

📊 Speedup:
  Module scan: 8.9x faster
  Executable scan: 7.1x faster
```

---

## 8. **Memory Comparison**

**Problem:** Find what changed after an action (e.g., taking damage).

```cpp
#include <iostream>
#include <thread>
#include <VolkDMA/dma.hh>
#include <VolkDMA/dumper.hh>

int main() {
    VolkDMA::DMA dma;
    dma.initialize();
    
    DWORD pid = dma.get_process_id("game.exe");
    
    // Dump before action
    std::cout << "📸 Taking snapshot BEFORE...\n";
    uint64_t range_start = 0x7FF700000000;
    size_t range_size = 1024 * 1024; // 1 MB
    
    dma.dump_memory_region(range_start, range_size, "before.bin", pid);
    
    // Wait for user action
    std::cout << "💡 Take damage in game, then press Enter...\n";
    std::cin.get();
    
    // Dump after action
    std::cout << "📸 Taking snapshot AFTER...\n";
    dma.dump_memory_region(range_start, range_size, "after.bin", pid);
    
    // Compare dumps
    std::cout << "🔍 Comparing dumps...\n";
    
    VolkDMA::MemoryDumper dumper;
    auto changes = dumper.compare_dumps("before.bin", "after.bin");
    
    std::cout << "✅ Found " << changes.size() << " changed addresses:\n\n";
    
    // Show first 10 changes
    for (size_t i = 0; i < std::min(changes.size(), size_t(10)); ++i) {
        uint64_t addr = range_start + changes[i];
        
        // Read current value
        uint32_t value = dma.read<uint32_t>(addr, pid);
        
        std::cout << "  0x" << std::hex << addr << ": " << std::dec << value << "\n";
    }
    
    if (changes.size() > 10) {
        std::cout << "  ... and " << (changes.size() - 10) << " more\n";
    }
    
    std::cout << "\n💡 Likely candidates for health address!\n";
    
    return 0;
}
```

**Expected Output:**
```
📸 Taking snapshot BEFORE...
💡 Take damage in game, then press Enter...
[User takes damage and presses Enter]

📸 Taking snapshot AFTER...
🔍 Comparing dumps...
✅ Found 47 changed addresses:

  0x7ff700123100: 100
  0x7ff700123456: 85   ← Health changed (100→85)!
  0x7ff700123800: 1234
  0x7ff700124000: 567
  ...

💡 Likely candidates for health address!
```

---

## 📚 **More Examples**

For more examples, see:
- [GETTING_STARTED.md](GETTING_STARTED.md) - Basic tutorial
- [API_REFERENCE.md](API_REFERENCE.md) - Complete API docs
- [FAQ.md](FAQ.md) - Troubleshooting common issues
- `testing_software/` - Interactive test suite

---

**Need help?** Open an issue on [GitHub](https://github.com/YourUsername/VolkDMA/issues)
