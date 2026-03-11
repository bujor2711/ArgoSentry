# Changelog

All notable changes to ArgoSentry will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned Features
See [ROADMAP.md](ROADMAP.md) for detailed future plans:
- Pattern Compilation (v2.4 candidate)
- Threading for signature scanning (v2.5 candidate)
- Mock Interface for testing (v2.6 candidate)

---

## [2.3.0] - 2026-03-11

### Added
- **Rate Limiting** - Configurable bandwidth limits to reduce detection risk
  - Thread-safe implementation with `std::mutex` and `std::atomic`
  - Builder integration: `.with_rate_limit(bytes_per_sec)`
  - Dynamic control: `enable_rate_limiting()`, `set_rate_limit()`
  - Test coverage: Test 12 in `test_dma.cpp`
- GitHub infrastructure (workflows, templates)
- Community guidelines (CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md)

### Changed
- README.md enhanced with badges, Quick Start, and comprehensive documentation links

### Fixed
- None

### Security
- Rate limiting helps prevent behavioral detection by anti-cheat systems

---

## [2.2.0] - 2026-03-10

### Added
- **Builder Pattern** - Fluent configuration interface
  - Type-safe configuration
  - Method chaining: `.with_cache()`, `.with_metrics()`, etc.
  - Backward compatible with direct constructor
- **Health Monitoring** - Automated system health checks
  - DMA device connectivity validation
  - Memory access verification

### Changed
- Recommended initialization method now uses Builder pattern

---

## [2.1.0] - 2026-03-09

### Added
- **Memory Diffing** - Track memory changes over time
  - Snapshot creation and comparison
  - Change detection with old/new values
  - Integration with DMA class
- **Advanced Caching** - Configurable TTL and size limits
  - Time-based cache invalidation
  - LRU eviction policy
  - Thread-safe cache operations

### Fixed
- Cache memory leak in long-running sessions

---

## [2.0.0] - 2026-03-08

### Added
- Complete rebranding from VolkDMA to ArgoSentry
- Namespace change: `ArgoSentry::`
- Enhanced documentation structure

### Changed
- File structure reorganization
- Include paths updated to `ArgoSentry/`

### Migration Guide
```cpp
// Old (VolkDMA):
#include <VolkDMA/dma.hh>
using namespace VolkDMA;

// New (ArgoSentry):
#include <ArgoSentry/dma.hh>
using namespace ArgoSentry;
```

---

## [1.0.0] - 2025-12-01

### Added
- Initial stable release
- Core DMA operations (read, write, scan)
- Process management
- Module enumeration
- Signature scanning with wildcards
- Input state detection
- Comprehensive examples

### Foundation
Based on VolkDMA by lyk64 with enhancements:
- RAII design
- Exception-based error handling
- Performance metrics
- Thread-safe operations

---

## Version History Summary

| Version | Date | Key Features |
|---------|------|--------------|
| **v2.3** | 2026-03-11 | Rate Limiting, GitHub Infrastructure |
| **v2.2** | 2026-03-10 | Builder Pattern, Health Monitoring |
| **v2.1** | 2026-03-09 | Memory Diffing, Advanced Caching |
| **v2.0** | 2026-03-08 | Rebranding to ArgoSentry |
| **v1.0** | 2025-12-01 | Initial stable release |

For detailed version history, see [IMPLEMENTED_FEATURES.md](IMPLEMENTED_FEATURES.md).

---

## [Links]

- **Repository:** https://github.com/bujor2711/ArgoSentry
- **Issues:** https://github.com/bujor2711/ArgoSentry/issues
- **Releases:** https://github.com/bujor2711/ArgoSentry/releases
- **Original VolkDMA:** https://github.com/lyk64/VolkDMA
