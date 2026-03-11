# ArgoSentry

[![Build Status](https://github.com/bujor2711/ArgoSentry/workflows/CI%20Build%20&%20Test/badge.svg)](https://github.com/bujor2711/ArgoSentry/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-2.3-blue.svg)](https://github.com/bujor2711/ArgoSentry/releases)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](https://www.microsoft.com/windows)

> **🚢 Forked from [VolkDMA](https://github.com/lyk64/VolkDMA)** - A powerful direct memory access library for memory analysis & manipulation, reverse engineering, and debugging.

**ArgoSentry** is an enhanced fork of the VolkDMA library, adding production-ready features while maintaining full compatibility with the original codebase.

---

## 🚀 Quick Start

### Prerequisites
- **OS:** Windows 10/11 (x64)
- **Compiler:** Visual Studio 2022+ or MSVC 2026
- **C++ Standard:** C++17 or later
- **Hardware:** DMA-capable FPGA device (Squirrel, 35T, or compatible)

### Installation

```bash
# Clone repository
git clone https://github.com/bujor2711/ArgoSentry.git
cd ArgoSentry

# Build (Release configuration)
msbuild ArgoSentry.sln /p:Configuration=Release /p:Platform=x64

# Copy required DLLs to your project
copy dlls\*.dll YourProject\
```

### Basic Usage

```cpp
#include <ArgoSentry/dma.hh>
#include <iostream>

using namespace ArgoSentry;

int main()
{
    try
    {
        // Initialize DMA with builder pattern
        auto dma = DMA::Builder()
            .with_cache(100 * 1024 * 1024)      // 100MB cache
            .with_rate_limit(1 * 1024 * 1024)  // 1 MB/s rate limiting
            .with_metrics(true)                 // Enable performance metrics
            .build();

        // Get process ID
        DWORD pid = dma->get_process_id("target.exe");
        std::cout << "Process ID: " << pid << "\n";

        // Read memory
        uint64_t address = 0x140000000;
        uint64_t value = dma->read_u64(address, pid);
        std::cout << "Value at 0x" << std::hex << address << ": 0x" << value << "\n";

        // Signature scanning with wildcards
        uint64_t pattern_addr = dma->find_signature(
            "48 8B 0D ? ? ? ?",  // Pattern with wildcards
            0x140000000,         // Start address
            0x141000000,         // End address
            pid
        );

        if (pattern_addr != 0)
        {
            std::cout << "Pattern found at: 0x" << std::hex << pattern_addr << "\n";
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
```

### More Examples
See the [`example/`](example/) folder for comprehensive examples:
- **`test_dma.cpp`** - Full test suite (12 interactive tests)
- **`simple_test.cpp`** - Basic read/write operations
- **`device_test.cpp`** - Hardware diagnostics
- **`list_processes.cpp`** - Process enumeration
- **`memory_map.cpp`** - Memory mapping examples

---

## 📜 Attribution & Features

This project is forked from **[VolkDMA by lyk64](https://github.com/lyk64/VolkDMA)**.

### ✨ ArgoSentry Enhancements

ArgoSentry builds upon the excellent foundation provided by the original VolkDMA library, adding:

**v2.3 (Current):**
- ✅ **Rate Limiting** - Protect against detection with configurable bandwidth limits
- ✅ **Thread-safe operations** - Production-ready concurrency support

**v2.2:**
- ✅ **Builder Pattern** - Fluent, type-safe configuration interface
- ✅ **Health Monitoring** - Automated system health checks

**v2.1:**
- ✅ **Memory Diffing** - Track memory changes over time with snapshots
- ✅ **Advanced Caching** - Configurable TTL and size limits

**v1.0+:**
- ✅ **Performance Metrics** - Detailed operation statistics
- ✅ **Error Handling** - Robust exception-based error management
- ✅ **RAII Design** - Automatic resource management

**Original Repository:** https://github.com/lyk64/VolkDMA  
**Full Changelog:** See [IMPLEMENTED_FEATURES.md](IMPLEMENTED_FEATURES.md)

---

## 📋 Core Features

### DMA Session Management
- **RAII DMA handle** - Automatic resource cleanup
- **Memory map bootstrapping** - Optional memory mapping
- **FPGA initialization** - Stable hardware preparation routine
- **Process enumeration** - PID lookup by name (single and list)
- **Signature scanning** - Pattern matching with wildcard support

### Memory Operations
- **Module management** - Metadata (base, size, path), enumeration
- **PE image dumping** - Extract in-memory executable images
- **Typed reads/writes** - Type-safe memory access (u8, u16, u32, u64)
- **Pointer chain resolution** - Follow multi-level pointers
- **Scatter operations** - Batch read/write for performance
- **Virtual-to-physical** - Address translation
- **CR3 fix** - Process-specific page table handling

### Input State (Kernel-derived)
- **Cursor position** - Real-time mouse coordinates
- **Key/button detection** - Detect pressed keys and mouse buttons
- **VK code mapping** - Built-in virtual key code to name table

---

## 📦 Included Binaries

To simplify both compilation and usage, all required binaries are included in this repository.

ArgoSentry requires the included custom **`vmm.lib`** and **`vmm.dll`**, which have been patched for compatibility with the **virtual-to-physical address translation** and **CR3 fix**.
Do **not** replace these files with stock versions unless you fully understand the patch and its implications.

When using this library, place `FTD3XX.dll`, `leechcore.dll`, and `vmm.dll` in the same directory as your executable.
All required DLLs are available in the [`dlls`](dlls) folder.

---

## 📚 Documentation

### Getting Started
- **[README.md](README.md)** - This file (overview & quick start)
- **[ROADMAP.md](ROADMAP.md)** - Future features and implementation status
- **[IMPLEMENTED_FEATURES.md](IMPLEMENTED_FEATURES.md)** - Complete version history (v1.0 - v2.3)

### Detailed Documentation
- **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)** - Comprehensive setup guide
- **[docs/API_REFERENCE.md](docs/API_REFERENCE.md)** - Complete API documentation
- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** - System design and internals
- **[docs/EXAMPLES.md](docs/EXAMPLES.md)** - Code examples and patterns
- **[docs/FAQ.md](docs/FAQ.md)** - Frequently asked questions
- **[docs/CONFIG_SYSTEM.md](docs/CONFIG_SYSTEM.md)** - Configuration options

### Community & Development
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - How to contribute (code, docs, testing)
- **[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)** - Community guidelines
- **[SECURITY.md](SECURITY.md)** - Security policy & vulnerability reporting

### GitHub Resources
- **[Issues](https://github.com/bujor2711/ArgoSentry/issues)** - Bug reports & feature requests
- **[Pull Requests](https://github.com/bujor2711/ArgoSentry/pulls)** - Code contributions
- **[Actions](https://github.com/bujor2711/ArgoSentry/actions)** - CI/CD build status
- **[Releases](https://github.com/bujor2711/ArgoSentry/releases)** - Download binaries

---

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Quick Contribution Guide:**
1. 🐛 **Report bugs** - Use issue templates
2. ✨ **Request features** - Check ROADMAP.md first
3. 📝 **Improve docs** - Fix typos, add examples
4. 🔧 **Submit code** - Fork, branch, PR with tests

**Good First Issues:**
- Documentation improvements
- Adding more examples
- Testing on different hardware setups
- Bug fixes (labeled `good-first-issue`)

---

## 👥 Contributors

**ArgoSentry Team:**
- **Maintainer:** [bujor2711](https://github.com/bujor2711)

**Original VolkDMA Contributors:**
- **Creator:** [lyk64](https://github.com/lyk64)
- [Stipulations](https://github.com/Stipulations)

---

## 🙏 Credits

This project builds upon and utilizes components from:
- **[LeechCore](https://github.com/ufrisk/LeechCore)** - DMA framework by [Ulf Frisk](https://github.com/ufrisk)
- **[MemProcFS](https://github.com/ufrisk/MemProcFS)** - Memory analysis framework by [Ulf Frisk](https://github.com/ufrisk)
- **[VolkDMA](https://github.com/lyk64/VolkDMA)** - Original library by [lyk64](https://github.com/lyk64)

---

## ⚖️ License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

**Key Points:**
- ✅ Free to use, modify, and distribute
- ✅ Commercial use allowed
- ✅ Attribution required
- ⚠️ No warranty provided
- ⚠️ Use at your own risk

---

## ⚠️ Legal Disclaimer

**ArgoSentry is a DMA library designed for legitimate purposes:**
- ✅ Security research and analysis
- ✅ Debugging and reverse engineering (with authorization)
- ✅ Educational purposes
- ✅ Hardware testing and diagnostics

**However:**
- ⚠️ Using this library to access game memory may violate Terms of Service
- ⚠️ Users are responsible for compliance with local laws and regulations
- ⚠️ The maintainers do NOT endorse or support unauthorized access or cheating
- ⚠️ Use at your own risk - the maintainers are not liable for misuse

**Responsible Use:**
Please use ArgoSentry ethically and legally. Respect software licenses, Terms of Service, and applicable laws.

---

## 📞 Support & Contact

- **GitHub Issues:** [Report bugs or request features](https://github.com/bujor2711/ArgoSentry/issues)
- **Discussions:** [Ask questions and share ideas](https://github.com/bujor2711/ArgoSentry/discussions) *(if enabled)*
- **Security:** See [SECURITY.md](SECURITY.md) for vulnerability reporting

---

**⭐ If you find ArgoSentry useful, please consider starring the repository!**
