# 🚀 VolkDMA - High-Performance Direct Memory Access Library

[![Version](https://img.shields.io/badge/version-1.9-blue.svg)](CHANGELOG.md)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-green.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/license-MIT-orange.svg)](../LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)](../ROADMAP.md)

**VolkDMA** is a production-ready C++17 library for high-performance Direct Memory Access (DMA) operations using FPGA hardware. Designed for game development, reverse engineering, debugging, and memory analysis.

---

## 📖 **What is VolkDMA?**

VolkDMA provides a simple, fast, and reliable way to **read process memory** via DMA hardware, bypassing traditional operating system security. It's used for:

- 🎮 **Game Development** - Read game state, player positions, entity data
- 🔍 **Reverse Engineering** - Analyze memory structures, find signatures
- 🐛 **Debugging** - Inspect memory without attaching debuggers
- 🔬 **Memory Analysis** - Dump memory regions, compare changes
- ⚡ **Performance** - 10-100x faster than traditional methods with caching

**Key Innovation:** Uses FPGA hardware to access memory directly, avoiding kernel-mode drivers and anti-cheat detection.

---

## ✨ **Key Features**

### **Core DMA Operations**
- ✅ **Memory Reading** - Read any data type from any process
- ✅ **Signature Scanning** - Find patterns with wildcards (e.g., `48 8B 05 ? ? ?`)
- ✅ **Process Enumeration** - Find processes by name
- ✅ **Type-Safe API** - Template-based reads with compile-time checks

### **Performance Features** ⚡
- ✅ **Memory Cache** - 10-100x speedup for repeated reads (v1.5)
- ✅ **Smart Scanning** - 80-90% faster signature scanning (v1.6)
- ✅ **Batch Operations** - 50-80% overhead reduction for multi-reads (v1.7)
- ✅ **Metrics System** - Real-time performance monitoring (v1.2)

### **Advanced Features** 🚀
- ✅ **Input Validation** - Comprehensive error checking (v1.1)
- ✅ **Health Monitoring** - FPGA status, auto-recovery (v1.8)
- ✅ **Memory Dumps** - 4 formats: Binary, HexDump, CArray, IDA (v1.9)
- ✅ **Configuration System** - Runtime settings via INI file (v1.4)

### **Developer Experience** 💻
- ✅ **Testing Suite** - Interactive testing software with 9 tests (v1.3)
- ✅ **Documentation** - Complete API reference and examples
- ✅ **Production-Ready** - Thread-safe, error-handled, tested

---

## ⚡ **Quick Start (5 Minutes)**

### **1. Installation**

```bash
# Clone repository
git clone https://github.com/yourusername/VolkDMA.git
cd VolkDMA

# Build with Visual Studio 2019+ or CMake
# Open VolkDMA.sln in Visual Studio and build
```

### **2. First Program**

```cpp
#include "VolkDMA/dma.hh"
#include <iostream>

int main() {
    try {
        // Initialize DMA
        VolkDMA::DMA dma(true); // true = use memory map

        // Find process
        DWORD pid = dma.get_process_id("notepad.exe");
        std::cout << "Found process: " << pid << "\n";

        // Read memory (example: read uint32_t at address 0x400000)
        uint32_t value = dma.read<uint32_t>(0x400000, pid);
        std::cout << "Value: 0x" << std::hex << value << "\n";

        // Find signature pattern
        uint64_t addr = dma.find_signature(
            "48 8B 05 ? ? ? ?",  // Pattern with wildcards
            0x140000000,          // Start address
            0x145000000,          // End address
            pid
        );
        std::cout << "Pattern found at: 0x" << std::hex << addr << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

### **3. Run**

```bash
# Compile
cl /std:c++17 /EHsc example.cpp VolkDMA.lib

# Run (requires FPGA hardware connected)
example.exe
```

**That's it!** 🎉 You're now reading memory via DMA.

---

## 📚 **Documentation**

| Document | Description |
|----------|-------------|
| [**Getting Started**](GETTING_STARTED.md) | Complete tutorial for beginners |
| [**API Reference**](API_REFERENCE.md) | Full API documentation for all classes |
| [**Examples**](EXAMPLES.md) | Copy-paste code for common use cases |
| [**FAQ**](FAQ.md) | Frequently asked questions and troubleshooting |
| [**Architecture**](ARCHITECTURE.md) | Internal design and how it works |
| [**Performance**](PERFORMANCE.md) | Optimization guide and benchmarks |
| [**Migration Guides**](MIGRATION_GUIDES.md) | Upgrade between versions |
| [**Contributing**](CONTRIBUTING.md) | How to contribute code |
| [**Glossary**](GLOSSARY.md) | Technical terms explained |

---

## 🔧 **Requirements**

### **Hardware**
- ✅ **FPGA Device** - PCILeech-compatible FPGA hardware
  - Recommended: Screamer PCIe Squirrel
  - Alternative: LambdaConcept PCIeScreamer
- ✅ **Host Computer** - Windows 10/11 x64
- ✅ **Target Computer** - Any Windows system (can be same computer)

### **Software**
- ✅ **Compiler** - MSVC 2019+ or GCC 9+ (C++17 required)
- ✅ **Dependencies** - `vmm.dll` (included in release)
- ✅ **Driver** - FPGA driver (auto-installed by setup)

### **Supported Platforms**
- ✅ Windows 10/11 x64 (**only platform currently supported**)
- ❌ Linux (planned, not implemented)
- ❌ macOS (no plans)

---

## 📦 **Installation**

### **Option 1: Pre-built Binary (Recommended)**

```bash
# Download latest release
wget https://github.com/yourusername/VolkDMA/releases/latest/VolkDMA.zip

# Extract
unzip VolkDMA.zip

# Copy to your project
cp VolkDMA/lib/VolkDMA.lib your_project/lib/
cp VolkDMA/include/* your_project/include/
```

### **Option 2: Build from Source**

```bash
# Clone repository
git clone https://github.com/yourusername/VolkDMA.git
cd VolkDMA

# Build with Visual Studio
# Open VolkDMA.sln and build (Release x64)

# Or build with CMake
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### **Option 3: Include Directly in Project**

```bash
# Add as Git submodule
git submodule add https://github.com/yourusername/VolkDMA.git external/VolkDMA

# Update .vcxproj or CMakeLists.txt
# Include directories: external/VolkDMA/include
# Link libraries: external/VolkDMA/lib/VolkDMA.lib
```

---

## 🎯 **Use Cases**

### **Game Hacking** 🎮
```cpp
// ESP (read player positions)
auto players = dma.batch_read_typed<Vec3>(player_addresses, pid);
for (const auto& pos : players) {
    if (pos) DrawESP(*pos);
}
```

### **Reverse Engineering** 🔍
```cpp
// Find pattern in specific module
uint64_t addr = dma.find_signature_in_module(
    "E8 ? ? ? ? 48 8B",  // Call instruction pattern
    "game.dll",
    pid
);
```

### **Debugging** 🐛
```cpp
// Dump memory region for analysis
dma.dump_memory_region(
    0x140000000, 0x141000000,
    "game_memory.bin",
    pid,
    VolkDMA::DumpFormat::IDA  // IDA Pro compatible
);
```

### **Memory Analysis** 🔬
```cpp
// Compare memory before/after action
dma.dump_memory_region(start, end, "before.bin", pid);
// ... user does something ...
dma.dump_memory_region(start, end, "after.bin", pid);

auto changed = dma.compare_memory_dumps("before.bin", "after.bin");
// changed contains addresses that changed
```

---

## 🚀 **Performance**

| Feature | Improvement | Version |
|---------|-------------|---------|
| **Memory Cache** | 10-100x speedup | v1.5 ⚡ |
| **Smart Scanning** | 80-90% time reduction | v1.6 🚀 |
| **Batch Operations** | 50-80% overhead reduction | v1.7 💪 |

**Real-world example (reading 100 entities):**
- Without cache: ~500ms
- With cache: ~5ms (100x faster!)

See [PERFORMANCE.md](PERFORMANCE.md) for benchmarks and optimization guide.

---

## 🎓 **Learning Path**

**For Beginners:**
1. Read [GETTING_STARTED.md](GETTING_STARTED.md) - Complete tutorial
2. Try [EXAMPLES.md](EXAMPLES.md) - Copy-paste working code
3. Check [FAQ.md](FAQ.md) - Common questions answered

**For Developers:**
1. Read [API_REFERENCE.md](API_REFERENCE.md) - Full API docs
2. Study [ARCHITECTURE.md](ARCHITECTURE.md) - How it works internally
3. Optimize with [PERFORMANCE.md](PERFORMANCE.md)

**For Contributors:**
1. Read [CONTRIBUTING.md](CONTRIBUTING.md) - Development guide
2. Check [ROADMAP.md](../ROADMAP.md) - Planned features

---

## ⚠️ **Legal & Ethical Considerations**

**This library can be used for:**
- ✅ Educational purposes (learning memory management, DMA)
- ✅ Personal projects (game mods, trainers)
- ✅ Security research (analyzing malware, testing anti-cheat)
- ✅ Debugging your own applications

**This library should NOT be used for:**
- ❌ Cheating in online multiplayer games (violates ToS)
- ❌ Unauthorized access to systems
- ❌ Piracy or bypassing copy protection
- ❌ Any illegal activities

**Disclaimer:** The authors are not responsible for misuse. Use responsibly and ethically.

---

## 🤝 **Contributing**

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Code style guide
- Pull request process
- Issue reporting
- Feature requests

**Quick Links:**
- [Report Bug](https://github.com/yourusername/VolkDMA/issues/new?template=bug_report.md)
- [Request Feature](https://github.com/yourusername/VolkDMA/issues/new?template=feature_request.md)
- [Ask Question](https://github.com/yourusername/VolkDMA/discussions)

---

## 📊 **Project Status**

**Current Version:** v1.9 - Memory Dump Utilities  
**Status:** ✅ **Production-Ready**  
**Last Updated:** March 11, 2026

**Implemented Features:** 9/9 core versions (v1.0 - v1.9)  
**Nice-to-Have Features:** 5/18 implemented (28%)

See [ROADMAP.md](../ROADMAP.md) for planned features and development timeline.

---

## 📜 **License**

This project is licensed under the **MIT License** - see [LICENSE](../LICENSE) file for details.

**Summary:**
- ✅ Free to use commercially
- ✅ Free to modify and distribute
- ✅ No warranty provided
- ⚠️ Must include license in distributions

---

## 💬 **Support**

**Get Help:**
- 📖 [Documentation](GETTING_STARTED.md)
- ❓ [FAQ](FAQ.md)
- 💬 [Discussions](https://github.com/yourusername/VolkDMA/discussions)
- 🐛 [Issues](https://github.com/yourusername/VolkDMA/issues)

**Community:**
- Discord: [Join Server](#)
- Reddit: [r/VolkDMA](#)
- Twitter: [@VolkDMA](#)

---

## 🏆 **Acknowledgments**

- **PCILeech** by Ulf Frisk - Inspiration for FPGA DMA
- **MemProcFS** - Memory management techniques
- **Community Contributors** - Bug reports and feature ideas

---

## 📈 **Changelog**

See [CHANGELOG.md](../CHANGELOG.md) for version history and changes.

**Recent Releases:**
- **v1.9** (Mar 2026) - Memory Dump Utilities (4 formats, comparison tools)
- **v1.8** (Mar 2026) - Health Monitoring (auto-recovery, diagnostics)
- **v1.7** (Mar 2026) - Batch Read Operations (50-80% faster)
- **v1.6** (Mar 2026) - Memory Layout Analysis (80-90% scan reduction)
- **v1.5** (Mar 2026) - Memory Cache (10-100x speedup)

---

## 🚀 **Quick Links**

| Resource | Link |
|----------|------|
| **Documentation** | [/docs](.) |
| **Examples** | [EXAMPLES.md](EXAMPLES.md) |
| **API Reference** | [API_REFERENCE.md](API_REFERENCE.md) |
| **Releases** | [GitHub Releases](https://github.com/yourusername/VolkDMA/releases) |
| **Roadmap** | [ROADMAP.md](../ROADMAP.md) |
| **Testing Software** | [/testing_software](../testing_software) |

---

<div align="center">

**Made with ❤️ by the VolkDMA Team**

[⭐ Star on GitHub](https://github.com/yourusername/VolkDMA) • [📝 Report Issue](https://github.com/yourusername/VolkDMA/issues) • [💬 Join Discussion](https://github.com/yourusername/VolkDMA/discussions)

</div>
