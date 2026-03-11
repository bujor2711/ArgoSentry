# ❓ VolkDMA FAQ (Frequently Asked Questions)

Common questions, problems, and solutions.

---

## 📋 **Table of Contents**

1. [General Questions](#general-questions)
2. [Installation Issues](#installation-issues)
3. [Usage Questions](#usage-questions)
4. [Troubleshooting](#troubleshooting)
5. [Advanced Topics](#advanced-topics)

---

## 🤔 **General Questions**

### **Q: What is DMA?**

**A:** DMA (Direct Memory Access) is a method of reading computer memory **directly from hardware**, bypassing the operating system. VolkDMA uses an FPGA device to read memory without being detected by anti-cheat systems.

```
Normal Read (Detectable):
Your Program → Windows APIs → Anti-Cheat ❌ → Memory

DMA Read (Undetectable):
Your Program → FPGA Hardware → Memory ✅
```

---

### **Q: Is this legal?**

**A:** **It depends on how you use it:**

✅ **Legal:**
- Personal education and research
- Debugging your own applications
- Security research
- Single-player games

❌ **Illegal/Unethical:**
- Online multiplayer cheating
- Circumventing anti-cheat for competitive advantage
- Unauthorized access to protected systems

**Always check the game's Terms of Service!**

---

### **Q: Can anti-cheat detect this?**

**A:** Traditional anti-cheats **cannot detect DMA** because it operates **outside the operating system**. However:
- ⚠️ Behavioral analysis can still detect you (e.g., perfect aim, inhuman reactions)
- ⚠️ Some games have **server-side** anti-cheat that detects impossible actions
- ⚠️ Always use responsibly and follow game rules

---

### **Q: What hardware do I need?**

**A:** You need:
1. **FPGA device** (PCILeech-compatible)
   - Recommended: Squirrel, DMA Card, Enigma
   - Price: $200-$800
2. **Two computers**:
   - **Host computer:** Runs your VolkDMA program
   - **Target computer:** Runs the game (FPGA installed here)
3. **Connection:** USB cable between FPGA and host computer

---

### **Q: Why is it so expensive?**

**A:** FPGA hardware is specialized and requires:
- Custom PCB design
- FPGA chip (expensive)
- Custom firmware
- Driver development

**Alternatives:**
- ❌ Software-only solutions are detectable
- ⚠️ Cheaper cards may have limited features or compatibility

---

### **Q: Do I need two computers?**

**A:** **Yes**, for most use cases:
- **Target computer:** Has FPGA installed, runs the game
- **Host computer:** Runs your VolkDMA program, analyzes data

**Exception:** Some FPGA devices support "loopback mode" for single-PC setup, but this is less common and potentially detectable.

---

## 🔧 **Installation Issues**

### **Q: "Failed to initialize DMA" error**

**Possible causes and solutions:**

**1. FPGA not connected**
```powershell
# Check FPGA connection
# Device Manager → Universal Serial Bus devices → Look for FPGA
```
✅ **Solution:** Reconnect FPGA USB cable, reinstall drivers

**2. Drivers not installed**
```powershell
# Test FPGA drivers
pcileech.exe testmemread -device fpga
```
✅ **Solution:** Install FPGA drivers from manufacturer

**3. Missing DLLs**
```
Error: vmm.dll not found
```
✅ **Solution:** Copy `vmm.dll` and `leechcore.dll` to your project directory

**4. Insufficient permissions**
```powershell
# Run as Administrator
Right-click your_program.exe → Run as administrator
```

---

### **Q: Compilation errors**

**Error:** `'DMA' is not a member of 'VolkDMA'`

✅ **Solution:** Include the correct header
```cpp
#include <VolkDMA/dma.hh>  // Correct
```

**Error:** `unresolved external symbol`

✅ **Solution:** Link against `VolkDMA.lib`
```cmake
target_link_libraries(your_program VolkDMA)
```

**Error:** `C++17 required`

✅ **Solution:** Enable C++17 in your project
```cmake
set(CMAKE_CXX_STANDARD 17)
```

---

### **Q: FPGA device not detected**

**Steps to diagnose:**

1. **Check physical connection**
   - FPGA properly seated in PCIe slot?
   - USB cable connected to host computer?

2. **Check Device Manager (Windows)**
   ```
   Device Manager → Universal Serial Bus devices
   ```
   - Should see FPGA device listed
   - If "Unknown Device", reinstall drivers

3. **Test with PCILeech**
   ```powershell
   pcileech.exe testmemread -device fpga
   ```
   - Should output: "Memory read test: OK"

4. **Check BIOS settings**
   - Enable "Above 4G Decoding"
   - Enable "Resizable BAR"
   - Disable "IOMMU" (VT-d)

---

## 💬 **Usage Questions**

### **Q: How do I find a process ID?**

```cpp
// By name (case-insensitive)
DWORD pid = dma.get_process_id("game.exe");

if (pid == 0) {
    std::cerr << "Process not found!\n";
}
```

**Common mistakes:**
- ❌ `"game"` (missing .exe)
- ❌ `"Game.exe"` (wrong case - but actually case-insensitive!)
- ✅ `"game.exe"` (correct)

---

### **Q: How do I create signature patterns?**

**Step 1:** Get bytes from disassembler (IDA Pro, Ghidra, x64dbg)

```asm
48 8B 05 12 34 56 78    MOV RAX, [RIP+0x78563412]
48 85 C0                TEST RAX, RAX
```

**Step 2:** Replace dynamic bytes with wildcards `?`

```cpp
"48 8B 05 ? ? ? ? 48 85 C0"
         ↑______↑  These bytes change between runs
```

**Rules:**
- Use `?` for dynamic values (offsets, addresses, counts)
- Keep static bytes (opcodes, registers)
- Separate bytes with spaces

---

### **Q: What wildcards mean in patterns?**

```cpp
"48 8B 05 ? ? ? ?"
```

- `48`, `8B`, `05` = **Exact byte match** (must be 0x48, 0x8B, 0x05)
- `?` = **Wildcard** (can be any byte 0x00-0xFF)

**Example:**
```cpp
Pattern: "E8 ? ? ? ?"
Matches: E8 10 20 30 40
Matches: E8 AA BB CC DD
Matches: E8 00 00 00 00
```

---

### **Q: How do I handle read errors?**

**Always use try-catch:**

```cpp
try {
    uint32_t value = dma.read<uint32_t>(address, pid);
    // Use value...
} catch (const std::exception& e) {
    std::cerr << "Read failed: " << e.what() << "\n";
    // Handle error...
}
```

**Common error messages:**
- `"Invalid process ID"` → Process not found or terminated
- `"Read failed: access violation"` → Address is invalid/unmapped
- `"DMA not initialized"` → Call `dma.initialize()` first

---

### **Q: Is VolkDMA thread-safe?**

**A:** **Yes**, most operations are thread-safe:

✅ **Thread-safe:**
- `read<T>()` - Multiple threads can read simultaneously
- `find_signature()` - Signature scanning
- `get_process_id()` - Process lookup
- `batch_read()` - Batch operations
- Cache operations

❌ **Not thread-safe:**
- `initialize()` - Call once from main thread

**Example:**
```cpp
// Multiple threads reading different addresses - OK ✅
std::thread t1([&]() { dma.read<int>(addr1, pid); });
std::thread t2([&]() { dma.read<int>(addr2, pid); });
```

---

## 🐛 **Troubleshooting**

### **Q: "Process not found" but I see it running**

**Possible causes:**

1. **Wrong process name**
   ```cpp
   // ❌ WRONG
   dma.get_process_id("game");      // Missing .exe
   dma.get_process_id("GAME.EXE");  // Wrong case (actually works, but good to check)
   
   // ✅ CORRECT
   dma.get_process_id("game.exe");
   ```

2. **Process on wrong computer**
   - Process must be on **target computer** (where FPGA is installed)
   - Not on host computer where your program runs

3. **32-bit vs 64-bit process**
   - VolkDMA only supports **64-bit processes**
   - Check Task Manager → Details → Look at "Platform" column

---

### **Q: "Access violation" when reading**

**Possible causes:**

1. **Invalid address**
   ```cpp
   // Don't use random addresses!
   uint32_t value = dma.read<uint32_t>(0x12345, pid); // ❌ Random address
   ```
   ✅ **Solution:** Use addresses from:
   - Memory dumps
   - Signature scans
   - Known offsets

2. **Unmapped memory**
   - Address exists but isn't mapped in target process
   ✅ **Solution:** Verify address range with memory analysis tools

3. **Process terminated**
   - Process crashed or exited
   ✅ **Solution:** Check if process still exists

---

### **Q: Signature scan is too slow**

**Problem:** Scanning 100+ MB takes 30 seconds

**Solutions:**

1. **Use smart scanning (v1.6)**
   ```cpp
   // ❌ Slow: Full memory scan
   dma.find_signature(pattern, 0x0, 0xFFFFFFFFFFFFFFFF, pid);
   
   // ✅ Fast: Module-specific (8-10x faster)
   dma.find_signature_in_module(pattern, "client.dll", pid);
   
   // ✅ Fast: Executable regions only (7-8x faster)
   dma.find_signature_in_executable(pattern, pid);
   ```

2. **Reduce scan range**
   ```cpp
   // ❌ Huge range
   dma.find_signature(pattern, 0x0, 0x7FFFFFFFFFFF, pid);
   
   // ✅ Specific module range
   dma.find_signature(pattern, 0x7FF700000000, 0x7FF702000000, pid);
   ```

3. **Use better patterns**
   ```cpp
   // ❌ Too generic (many matches)
   "48 8B"
   
   // ✅ More specific (fewer false positives)
   "48 8B 0D ? ? ? ? 48 85 C9 74"
   ```

---

### **Q: Cache not working / no speedup**

**Possible causes:**

1. **Cache not enabled**
   ```cpp
   auto& cache = dma.get_cache();
   cache.set_enabled(true);  // Must enable!
   ```

2. **Reading different addresses each time**
   ```cpp
   // ❌ No cache benefit (always different addresses)
   for (int i = 0; i < 1000; ++i) {
       dma.read<int>(base_addr + i * 4, pid);
   }
   
   // ✅ Cache benefit (same address repeated)
   for (int i = 0; i < 1000; ++i) {
       dma.read<int>(health_addr, pid);
   }
   ```

3. **TTL expired**
   - Cache entries expire after 30 seconds (default)
   - Configure TTL if needed:
   ```cpp
   // Increase TTL to 60 seconds
   // (requires config file modification)
   ```

---

### **Q: FPGA disconnects randomly**

**Possible causes:**

1. **USB power saving**
   ```
   Device Manager → USB Hub → Properties → Power Management
   → Uncheck "Allow computer to turn off this device"
   ```

2. **Loose connection**
   - Check PCIe slot (target computer)
   - Check USB cable (host computer)

3. **Hardware issue**
   - Test with different USB port
   - Test with different computer

4. **Monitor with health system (v1.8)**
   ```cpp
   dma.start_automatic_health_monitoring(std::chrono::seconds(30));
   // Will alert if FPGA disconnects
   ```

---

## 🎓 **Advanced Topics**

### **Q: Can I read custom structs?**

**A:** **Yes**, if the struct is **trivially copyable:**

```cpp
// ✅ GOOD: Simple struct
struct Vec3 {
    float x, y, z;
};
Vec3 pos = dma.read<Vec3>(address, pid);

// ✅ GOOD: Fixed-size array
struct PlayerData {
    char name[64];
    int health;
    float position[3];
};
PlayerData data = dma.read<PlayerData>(address, pid);

// ❌ BAD: Contains pointers
struct BadStruct {
    int* pointer;  // Pointer won't be valid!
    std::string name;  // std::string has internal pointers!
};
```

**Rule:** Only use structs with **value types** (int, float, char[], fixed arrays).

---

### **Q: How do I use VolkDMA without FPGA (testing)?**

**A:** Use the **testing software** with mock implementation:

```powershell
cd testing_software
VolkDMA_Tester.exe
# Select test to run
```

The testing software has a **standalone mock DMA** that simulates memory without hardware.

**For your own tests:**
- See `testing_software/src/dma.cpp` for mock implementation example
- Create mock memory with `std::unordered_map<uint64_t, std::vector<uint8_t>>`

---

### **Q: Can I use multiple DMA instances?**

**A:** **Yes**, each `DMA` instance is independent:

```cpp
// Two separate DMA instances
VolkDMA::DMA dma1;
VolkDMA::DMA dma2;

dma1.initialize();
dma2.initialize();

// Read from different processes
DWORD pid1 = dma1.get_process_id("game1.exe");
DWORD pid2 = dma2.get_process_id("game2.exe");
```

**Note:** All instances share the same FPGA hardware, so performance may be impacted.

---

### **Q: How do I integrate VolkDMA into my project?**

**Method 1: Pre-built library**
```cmake
# CMakeLists.txt
find_library(VOLKDMA_LIB VolkDMA PATHS "path/to/volkdma/lib")
target_link_libraries(your_program ${VOLKDMA_LIB})
target_include_directories(your_program PRIVATE "path/to/volkdma/include")
```

**Method 2: Git submodule**
```bash
git submodule add https://github.com/YourUsername/VolkDMA.git external/volkdma
```
```cmake
add_subdirectory(external/volkdma)
target_link_libraries(your_program VolkDMA)
```

---

### **Q: What's the maximum read size?**

**A:** Default limit is **2 GB** per read (configurable).

```cpp
// Large read (1 MB)
auto data = dma.read_bytes(address, 1024 * 1024, pid);  // ✅ OK

// Very large read (3 GB)
auto data = dma.read_bytes(address, 3ULL * 1024 * 1024 * 1024, pid);  // ❌ May fail
```

**For very large reads:**
- Use **batch operations** to split into chunks
- Adjust configuration if needed

---

### **Q: Can I write memory?**

**A:** **Not yet.** VolkDMA v1.9 only supports **read operations**.

**Planned for future:**
- v2.0 may add write operations
- See [ROADMAP.md](../ROADMAP.md) for planned features

**Why read-only?**
- Safer (can't accidentally crash target process)
- Less detectable (no memory modifications)
- Sufficient for most use cases (ESPs, radars, information gathering)

---

## 🆘 **Still Need Help?**

### **Documentation**
- [GETTING_STARTED.md](GETTING_STARTED.md) - Beginner tutorial
- [API_REFERENCE.md](API_REFERENCE.md) - Complete API docs
- [EXAMPLES.md](EXAMPLES.md) - Copy-paste code examples
- [PERFORMANCE.md](PERFORMANCE.md) - Optimization guide

### **Interactive Testing**
```powershell
cd testing_software
VolkDMA_Tester.exe
# Run diagnostics and tests
```

### **Community**
- 🐛 [GitHub Issues](https://github.com/YourUsername/VolkDMA/issues) - Report bugs
- 💬 [GitHub Discussions](https://github.com/YourUsername/VolkDMA/discussions) - Ask questions
- 📧 Email: support@volkdma.example.com

### **Enable Debug Logging**
```cpp
dma.enable_metrics(true);
dma.log_metrics_detailed();
// Will print detailed information about operations
```

---

**Can't find your question?** [Open an issue](https://github.com/YourUsername/VolkDMA/issues/new) and we'll add it to the FAQ!
