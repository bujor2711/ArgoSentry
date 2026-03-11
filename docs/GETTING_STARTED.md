# 🚀 Getting Started with VolkDMA

Welcome! This guide will take you from **zero knowledge** to **writing your first working DMA program** in about **30 minutes**.

---

## 📋 **Table of Contents**

1. [Prerequisites](#prerequisites)
2. [Understanding Core Concepts](#understanding-core-concepts)
3. [Installation Guide](#installation-guide)
4. [Your First Program](#your-first-program)
5. [Understanding the Code](#understanding-the-code)
6. [Next Steps](#next-steps)

---

## ✅ **Prerequisites**

### **What You Need to Know**

- **Basic C++** - Variables, functions, classes, pointers
- **Basic command line** - Running commands, navigating directories
- **Basic understanding of processes** - What is a running program

### **What You DON'T Need to Know**

- ❌ You don't need to know what DMA is (we'll explain)
- ❌ You don't need hardware experience (we'll guide you)
- ❌ You don't need Windows internals knowledge (we abstract it)

### **What You Need to Have**

- ✅ **Windows 10 or 11** (64-bit)
- ✅ **FPGA device** (PCILeech-compatible)
- ✅ **Visual Studio 2019+** or **CMake 3.15+**
- ✅ **FPGA drivers** installed
- ✅ **vmm.dll** and **leechcore.dll** (included with FPGA software)

---

## 🧠 **Understanding Core Concepts**

Before we write code, let's understand what we're working with.

### **What is DMA (Direct Memory Access)?**

```
Normal Memory Read (Detectable):
┌─────────────┐
│ Your Program│
└──────┬──────┘
       │ Read request
       ▼
┌─────────────┐
│   Windows   │ ◄── Anti-cheat sees this!
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Game Memory │
└─────────────┘

DMA Memory Read (Undetectable):
┌─────────────┐
│ Your Program│
└──────┬──────┘
       │ DMA request
       ▼
┌─────────────┐
│ FPGA Device │ ◄── Bypasses Windows!
└──────┬──────┘
       │ Direct hardware access
       ▼
┌─────────────┐
│ Game Memory │
└─────────────┘
```

**DMA** = Reading memory **directly** from hardware, **bypassing the operating system**.

**Why is this useful?**
- ✅ Undetectable by anti-cheat (doesn't touch Windows APIs)
- ✅ Fast (hardware-speed access)
- ✅ Reliable (no interference from target process)

### **What is FPGA?**

**FPGA** (Field-Programmable Gate Array) = A hardware device that acts as a "memory bridge"

```
Your Computer (Host)         Target Computer (Game)
┌──────────────────┐        ┌──────────────────┐
│  VolkDMA Program │        │   Game Process   │
└────────┬─────────┘        └────────┬─────────┘
         │                           │
         │ USB/Thunderbolt           │
         ▼                           │
    ┌────────┐                       │
    │  FPGA  │◄──────────────────────┘
    └────────┘   PCIe connection
```

The FPGA sits **inside the target computer** and reads memory **directly from RAM**.

### **What is Signature Scanning?**

**Signature** = A pattern of bytes used to find code/data in memory

```cpp
// Example: Finding game's entity list
// Signature: "48 8B 0D ? ? ? ? 48 85 C9"
// Translation: MOV RCX, [RIP+???]  ; Load pointer
//              TEST RCX, RCX       ; Check if null

// "?" = wildcard (any byte)
```

**Why use signatures?**
- ✅ Game updates change addresses
- ✅ Signatures find code patterns (more stable)
- ✅ Works across different game versions

---

## 📦 **Installation Guide**

### **Step 1: Check Requirements**

```powershell
# Check Windows version (must be 10 or 11, 64-bit)
winver

# Check Visual Studio installed
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

# If file exists, you have Visual Studio ✅
```

### **Step 2: Setup FPGA Hardware**

1. **Connect FPGA to target computer**
   - Install FPGA into PCIe slot (or use Thunderbolt)
   - Power on target computer
   - FPGA should be detected (check Device Manager)

2. **Install FPGA drivers**
   - Download from your FPGA manufacturer
   - Run installer as Administrator
   - Restart computer if prompted

3. **Connect FPGA to host computer**
   - USB cable from FPGA to your development PC
   - Install USB drivers if prompted

4. **Test FPGA connection**
   ```powershell
   # Run PCILeech (comes with FPGA software)
   pcileech.exe testmemread -device fpga
   
   # Should output: "Memory read test: OK ✅"
   ```

### **Step 3: Download VolkDMA**

**Option A: Download Release (Recommended)**
```powershell
# Download from GitHub releases
https://github.com/YourUsername/VolkDMA/releases/latest

# Extract to C:\VolkDMA\
```

**Option B: Clone Repository**
```powershell
git clone https://github.com/YourUsername/VolkDMA.git
cd VolkDMA
```

### **Step 4: Copy Dependencies**

```powershell
# Copy vmm.dll and leechcore.dll to your project
# These come with your FPGA software (usually in C:\Program Files\PCILeech\)

copy "C:\Program Files\PCILeech\vmm.dll" "C:\VolkDMA\"
copy "C:\Program Files\PCILeech\leechcore.dll" "C:\VolkDMA\"
```

### **Step 5: Build VolkDMA**

**Using Visual Studio:**
```powershell
# Open VolkDMA.sln in Visual Studio
# Press Ctrl+Shift+B to build
# Output: VolkDMA.lib in x64/Release/
```

**Using CMake:**
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### **Step 6: Verify Installation**

Run the test program:
```powershell
cd testing_software
VolkDMA_Tester.exe

# Should show:
# ✅ VolkDMA Test Suite v1.9
# ✅ Select a test: [1-9]
```

**If you see this, installation successful! 🎉**

---

## 💻 **Your First Program**

Let's write a simple program that:
1. Initializes DMA
2. Finds a process (e.g., `notepad.exe`)
3. Reads memory from that process
4. Scans for a signature pattern

### **Complete Code (Copy-Paste Ready)**

```cpp
#include <iostream>
#include <VolkDMA/dma.hh>

int main() {
    try {
        // Step 1: Initialize DMA connection to FPGA
        std::cout << "Initializing DMA...\n";
        VolkDMA::DMA dma;
        
        if (!dma.initialize()) {
            std::cerr << "❌ Failed to initialize DMA!\n";
            std::cerr << "Check: FPGA connected? Drivers installed?\n";
            return 1;
        }
        std::cout << "✅ DMA initialized successfully!\n\n";

        // Step 2: Find target process by name
        std::cout << "Finding process 'notepad.exe'...\n";
        DWORD process_id = dma.get_process_id("notepad.exe");
        
        if (process_id == 0) {
            std::cerr << "❌ Process not found!\n";
            std::cerr << "Make sure notepad.exe is running.\n";
            return 1;
        }
        std::cout << "✅ Found process: PID = " << process_id << "\n\n";

        // Step 3: Read memory at a known address
        // (Replace with actual address from your use case)
        uint64_t address = 0x7FF700000000; // Example address
        std::cout << "Reading 4 bytes from address 0x" << std::hex << address << "...\n";
        
        uint32_t value = dma.read<uint32_t>(address, process_id);
        std::cout << "✅ Value at address: 0x" << std::hex << value << "\n\n";

        // Step 4: Scan for a signature pattern
        std::cout << "Scanning for pattern '48 8B 05 ? ? ? ?' in range...\n";
        
        uint64_t range_start = 0x7FF700000000;
        uint64_t range_end   = 0x7FF701000000; // 16 MB range
        
        uint64_t found_address = dma.find_signature(
            "48 8B 05 ? ? ? ?",  // Pattern with wildcards
            range_start,
            range_end,
            process_id
        );
        
        if (found_address != 0) {
            std::cout << "✅ Pattern found at: 0x" << std::hex << found_address << "\n";
        } else {
            std::cout << "❌ Pattern not found in range.\n";
        }

        // Step 5: Cleanup
        std::cout << "\n✅ Program completed successfully!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << "\n";
        return 1;
    }
}
```

### **How to Compile and Run**

```powershell
# Compile (Visual Studio)
cl /EHsc /std:c++17 /I"C:\VolkDMA\include" my_first_program.cpp /link VolkDMA.lib

# Or use CMakeLists.txt:
# add_executable(my_first_program my_first_program.cpp)
# target_link_libraries(my_first_program VolkDMA)

# Run
my_first_program.exe
```

### **Expected Output**

```
Initializing DMA...
✅ DMA initialized successfully!

Finding process 'notepad.exe'...
✅ Found process: PID = 12345

Reading 4 bytes from address 0x7FF700000000...
✅ Value at address: 0x12345678

Scanning for pattern '48 8B 05 ? ? ? ?' in range...
✅ Pattern found at: 0x7FF700ABC123

✅ Program completed successfully!
```

---

## 📖 **Understanding the Code**

Let's break down each part:

### **1. Include Headers**
```cpp
#include <VolkDMA/dma.hh>
```
- This is the main VolkDMA header
- Contains the `DMA` class with all functionality

### **2. Initialize DMA**
```cpp
VolkDMA::DMA dma;
if (!dma.initialize()) {
    // Handle error
}
```
- Creates DMA object
- `initialize()` connects to FPGA hardware
- Returns `true` if successful, `false` if failed
- **Why it fails:** FPGA not connected, drivers missing, hardware issue

### **3. Find Process**
```cpp
DWORD process_id = dma.get_process_id("notepad.exe");
```
- Searches for process by **exact name** (case-insensitive)
- Returns process ID (PID) if found, `0` if not found
- **Note:** Target process must be running on **target computer**, not host

### **4. Read Memory**
```cpp
uint32_t value = dma.read<uint32_t>(address, process_id);
```
- Reads 4 bytes (`uint32_t`) from `address` in target process
- Template function: `dma.read<Type>(address, pid)`
- Supported types: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `float`, `double`, custom structs

### **5. Scan for Signature**
```cpp
uint64_t found = dma.find_signature("48 8B 05 ? ? ? ?", start, end, pid);
```
- Searches for byte pattern in memory range `[start, end)`
- `?` = wildcard (matches any byte)
- Returns address where pattern found, or `0` if not found
- **Pattern format:** Hex bytes separated by spaces, `?` for wildcards

### **6. Error Handling**
```cpp
try {
    // DMA operations
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```
- All DMA operations can throw exceptions
- Always wrap in `try-catch` for production code
- Exception messages describe the error

---

## 🎯 **Next Steps**

Congratulations! 🎉 You've written your first DMA program!

### **Where to Go Next?**

**For More Examples:**
- 📖 [EXAMPLES.md](EXAMPLES.md) - Copy-paste code for common use cases

**To Learn the Full API:**
- 📖 [API_REFERENCE.md](API_REFERENCE.md) - Complete documentation of all methods

**Having Problems?**
- 📖 [FAQ.md](FAQ.md) - Solutions to common issues

**Want Better Performance?**
- 📖 [PERFORMANCE.md](PERFORMANCE.md) - Optimization guide with benchmarks

**Want to Understand How It Works?**
- 📖 [ARCHITECTURE.md](ARCHITECTURE.md) - Internal design explanation

### **Practice Projects**

Start with these beginner-friendly projects:

1. **Memory Monitor**
   - Read a game's health value every second
   - Print changes to console
   - Teaches: Polling, continuous reading

2. **Entity ESP**
   - Find entity list signature
   - Read all entity positions
   - Print coordinates
   - Teaches: Batch reading, data structures

3. **Signature Database**
   - Create library of common game patterns
   - Scan and save results
   - Teaches: Pattern management, persistence

---

## ⚠️ **Common Beginner Mistakes**

### **1. Reading from Wrong Process**
```cpp
// ❌ WRONG: Reading from host computer
dma.read<uint32_t>(address, process_id);

// ✅ CORRECT: Process must be on TARGET computer
// (where FPGA is installed)
```

### **2. Invalid Address**
```cpp
// ❌ WRONG: Random address
uint64_t address = 0x12345;

// ✅ CORRECT: Use valid addresses from:
// - Memory dumps
// - Signature scans
// - Known offsets
```

### **3. Wrong Pattern Format**
```cpp
// ❌ WRONG: No spaces
dma.find_signature("488B05????????", ...);

// ✅ CORRECT: Spaces between bytes
dma.find_signature("48 8B 05 ? ? ? ?", ...);
```

### **4. Forgetting Error Handling**
```cpp
// ❌ WRONG: No error check
uint32_t value = dma.read<uint32_t>(address, pid);

// ✅ CORRECT: Check for exceptions
try {
    uint32_t value = dma.read<uint32_t>(address, pid);
} catch (const std::exception& e) {
    std::cerr << "Read failed: " << e.what() << "\n";
}
```

---

## 🆘 **Getting Help**

### **If Something Doesn't Work:**

1. **Check the FAQ:** [FAQ.md](FAQ.md)
2. **Run diagnostics:**
   ```powershell
   cd testing_software
   VolkDMA_Tester.exe
   # Run Test 1: DMA Initialization
   ```
3. **Enable detailed logging:**
   ```cpp
   dma.enable_metrics(true);  // Enable performance tracking
   dma.log_metrics_detailed(); // See what's happening
   ```
4. **Ask for help:** [GitHub Issues](https://github.com/YourUsername/VolkDMA/issues)

---

## 📚 **Summary**

You've learned:
- ✅ What DMA is and why it's useful
- ✅ How FPGA hardware works
- ✅ How to install and setup VolkDMA
- ✅ How to write your first DMA program
- ✅ How to read memory and scan signatures
- ✅ Common mistakes to avoid

**Next:** Check out [EXAMPLES.md](EXAMPLES.md) for more practical code! 🚀

---

**Need help?** Open an issue on [GitHub](https://github.com/YourUsername/VolkDMA/issues)  
**Want to contribute?** See [CONTRIBUTING.md](CONTRIBUTING.md)  
**Have questions?** Check [FAQ.md](FAQ.md)
