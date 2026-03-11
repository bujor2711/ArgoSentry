# ArgoSentry

> **🚢 Forked from [VolkDMA](https://github.com/lyk64/VolkDMA)** - A powerful direct memory access library for memory analysis & manipulation, reverse engineering, and debugging.

**ArgoSentry** is an enhanced fork of the VolkDMA library, adding production-ready features while maintaining full compatibility with the original codebase.

## 📜 Attribution

This project is forked from **[VolkDMA by lyk64](https://github.com/lyk64/VolkDMA)**.

ArgoSentry builds upon the excellent foundation provided by the original VolkDMA library, adding additional features such as:
- ✅ **Rate Limiting** (v2.3) - Protect against detection with configurable bandwidth limits
- ✅ **Memory Diffing** (v2.1) - Track memory changes over time
- ✅ **Builder Pattern** (v2.2) - Fluent configuration interface
- ✅ **Health Monitoring** - Automated system health checks
- ✅ **Advanced Caching** - Configurable TTL and size limits
- ✅ **Performance Metrics** - Detailed operation statistics

**Original Repository:** https://github.com/lyk64/VolkDMA

---

### Currently supports:
- **DMA session management**
  - RAII DMA handle
  - Optional memory map bootstrapping and dumping
  - FPGA prepping routine for stable initialization
  - PID lookup (single and list by name)
  - Signature scanning in a given VA range with wildcard support

- **Process memory & modules**
  - Module metadata (base, size, path), enumeration, and in-memory PE image dumping
  - Typed reads/writes and pointer-chain reads
  - Creating/executing/closing scatter handles
  - Preparing scatter reads/writes
  - Virtual-to-physical address translation
  - CR3 fix

- **Input state (kernel-derived)**
  - Cursor position
  - Detecting pressed keys and mouse buttons
  - Built-in VK code to name table

## Included Binaries

To simplify both compilation and usage, all required binaries are included in this repository.

ArgoSentry requires the included custom **`vmm.lib`** and **`vmm.dll`**, which have been patched for compatibility with the **virtual-to-physical address translation** and **CR3 fix**.
Do **not** replace these files with stock versions unless you fully understand the patch and its implications.

When using this library, place `FTD3XX.dll`, `leechcore.dll`, and `vmm.dll` in the same directory as your executable.
All required DLLs are available in the [`dlls`](dlls) folder.

## Contributors

**ArgoSentry Team:**
- **Maintainer:** [bujor2711](https://github.com/bujor2711)

**Original VolkDMA Contributors:**
- **Creator:** [lyk64](https://github.com/lyk64)
- [Stipulations](https://github.com/Stipulations)

## Credits
This project builds upon and utilizes components from [LeechCore](https://github.com/ufrisk/LeechCore) and [MemProcFS](https://github.com/ufrisk/MemProcFS), both created by [Ulf Frisk](https://github.com/ufrisk).

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
