# Contributing to ArgoSentry

First off, thank you for considering contributing to ArgoSentry! 🎉

## 📜 Table of Contents
1. [Code of Conduct](#code-of-conduct)
2. [How Can I Contribute?](#how-can-i-contribute)
3. [Development Setup](#development-setup)
4. [Coding Standards](#coding-standards)
5. [Commit Guidelines](#commit-guidelines)
6. [Pull Request Process](#pull-request-process)
7. [Testing Requirements](#testing-requirements)

---

## 📖 Code of Conduct

This project adheres to the Contributor Covenant [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

---

## 🤝 How Can I Contribute?

### 🐛 Reporting Bugs
- Use GitHub Issues with the **bug report template**
- Search existing issues first to avoid duplicates
- Include: OS, compiler version, hardware setup
- Provide minimal reproducible example
- Attach error messages and stack traces

### ✨ Suggesting Features
- Use GitHub Issues with the **feature request template**
- Check `ROADMAP.md` first - feature might already be planned
- Explain the use case and expected behavior
- Consider implementation complexity and effort

### 📝 Improving Documentation
- Fix typos, clarify confusing sections
- Add examples or tutorials
- Update outdated information
- Translate documentation (optional)

### 🔧 Code Contributions
- Pick an issue labeled `good-first-issue` or `help-wanted`
- Discuss major changes in an issue first
- Follow the coding standards below
- Add tests for new features
- Update documentation

---

## 🛠️ Development Setup

### Prerequisites
- **Windows 10/11** (x64)
- **Visual Studio 2022+** or MSVC 2026
- **Git** for version control
- **DMA Hardware** (Squirrel, 35T, or compatible FPGA)

### Setup Steps
```bash
# 1. Fork the repository on GitHub

# 2. Clone your fork
git clone https://github.com/YOUR_USERNAME/ArgoSentry.git
cd ArgoSentry

# 3. Add upstream remote
git remote add upstream https://github.com/bujor2711/ArgoSentry.git

# 4. Create a feature branch
git checkout -b feature/your-feature-name

# 5. Initialize submodules (if any)
git submodule update --init --recursive

# 6. Open in Visual Studio
start ArgoSentry.sln
```

### Building
```bash
# Debug build
msbuild ArgoSentry.sln /p:Configuration=Debug /p:Platform=x64

# Release build
msbuild ArgoSentry.sln /p:Configuration=Release /p:Platform=x64
```

### Running Tests
```bash
cd example
.\TestDMA.exe
# Select test number and verify output
```

---

## 📏 Coding Standards

### C++ Style Guide

#### Naming Conventions
```cpp
// Classes: PascalCase
class RateLimiter { };
class CompiledPattern { };

// Functions/Methods: snake_case
void read_memory();
uint64_t find_signature();

// Variables: snake_case
size_t byte_count;
uint64_t base_address;

// Constants: UPPER_SNAKE_CASE
constexpr size_t MAX_CACHE_SIZE = 100 * 1024 * 1024;

// Private members: trailing underscore
private:
    std::mutex mutex_;
    size_t bytes_consumed_;
```

#### Formatting
- **Indentation:** 4 spaces (NO tabs)
- **Line length:** Max 100-120 characters
- **Braces:** Allman style (opening brace on new line)
```cpp
void function_name()
{
    if (condition)
    {
        // code
    }
}
```

#### Modern C++ Features (C++17)
- ✅ Use `[[nodiscard]]` for return values
- ✅ Use `std::optional` for nullable returns
- ✅ Use `auto` where type is obvious
- ✅ Use RAII for resource management
- ✅ Prefer `std::unique_ptr`/`std::shared_ptr` over raw pointers
- ✅ Use `constexpr` where possible
- ❌ NO `std::vector<bool>` (use `std::vector<uint8_t>`)

#### Documentation
```cpp
/**
 * @brief Short description of function
 * @param address Memory address to read from
 * @param pid Process ID
 * @return Value read from memory
 * @throws std::runtime_error if read fails
 * 
 * Detailed description if needed.
 * Multiple lines are OK.
 */
[[nodiscard]] uint64_t read_u64(uint64_t address, DWORD pid);
```

#### Error Handling
```cpp
// ✅ DO: Use exceptions for error conditions
if (!validate_address(addr))
{
    throw std::invalid_argument("Invalid address: 0x" + std::to_string(addr));
}

// ✅ DO: Use std::optional for "not found" scenarios
[[nodiscard]] std::optional<ModuleInfo> get_module(const char* name);

// ❌ DON'T: Return magic values (0, -1, nullptr without documentation)
```

#### Thread Safety
```cpp
// ✅ DO: Document thread safety
/**
 * @brief Thread-safe read operation
 * @threadsafe Multiple threads can call this simultaneously
 */
void read_memory() const;

// ✅ DO: Use appropriate synchronization
mutable std::mutex mutex_;           // For exclusive access
mutable std::shared_mutex r_mutex_;  // For read-write locks
std::atomic<size_t> counter_{0};    // For lock-free counters
```

---

## 📝 Commit Guidelines

### Commit Message Format
```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types
- **feat:** New feature
- **fix:** Bug fix
- **docs:** Documentation changes
- **style:** Code style changes (formatting, no logic change)
- **refactor:** Code refactoring (no functional change)
- **perf:** Performance improvements
- **test:** Adding or updating tests
- **chore:** Maintenance tasks (build, CI, dependencies)

### Examples
```bash
feat(cache): Add LRU eviction policy

Implement least-recently-used eviction when cache reaches size limit.
This prevents unbounded memory growth.

Closes #42

---

fix(rate_limiter): Fix race condition in multi-threaded access

Add std::mutex to protect bytes_consumed_ counter.
Without this, concurrent threads could bypass rate limit.

Fixes #38

---

docs(readme): Add Quick Start section with code examples

Improves onboarding for new users by providing immediate
usage examples without reading full documentation.
```

### Commit Best Practices
- ✅ One logical change per commit
- ✅ Clear, concise subject line (<50 chars)
- ✅ Detailed body explaining "why" (not "what")
- ✅ Reference related issues
- ❌ NO "WIP" or "fix" commits (squash before PR)

---

## 🔄 Pull Request Process

### Before Submitting
1. ✅ **Update your fork:**
   ```bash
   git fetch upstream
   git rebase upstream/master
   ```

2. ✅ **Ensure builds pass:**
   ```bash
   msbuild ArgoSentry.sln /p:Configuration=Debug /p:Platform=x64
   msbuild ArgoSentry.sln /p:Configuration=Release /p:Platform=x64
   ```

3. ✅ **Run tests:**
   ```bash
   cd example
   .\TestDMA.exe
   # Verify all relevant tests pass
   ```

4. ✅ **Update documentation:**
   - Update README.md if API changed
   - Update ROADMAP.md if feature was planned
   - Add/update inline code comments

5. ✅ **Clean commit history:**
   ```bash
   # Squash WIP commits
   git rebase -i HEAD~5
   ```

### Submitting the PR
1. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```

2. Open Pull Request on GitHub
   - Fill out the PR template completely
   - Reference related issues
   - Add screenshots if UI changes

3. Respond to review feedback
   - Address all comments
   - Push additional commits if needed
   - Request re-review when ready

### PR Review Criteria
- ✅ Code follows style guide
- ✅ All tests pass
- ✅ No compiler warnings
- ✅ Documentation updated
- ✅ No merge conflicts
- ✅ Commit messages follow guidelines

---

## 🧪 Testing Requirements

### For Bug Fixes
- ✅ Add test that reproduces the bug
- ✅ Verify test fails before fix
- ✅ Verify test passes after fix
- ✅ Test edge cases

### For New Features
- ✅ Unit tests for core functionality
- ✅ Integration tests with DMA hardware (if applicable)
- ✅ Test error conditions
- ✅ Test thread safety (if multi-threaded)
- ✅ Benchmark performance (if performance-critical)

### Testing with MockDMA (when implemented)
```cpp
// Example unit test structure
TEST(FeatureTest, BasicFunctionality)
{
    MockDMA mock;
    mock.set_memory(0x140000000, {0x48, 0x8B, 0x0D});
    
    // Test your feature
    auto result = your_feature(mock);
    
    EXPECT_TRUE(result.success());
}
```

---

## 🚫 What NOT to Contribute

### ❌ Prohibited Contributions
- **Memory write operations** - ToS violation, high detection risk
- **Anti-debugging** - Increases detection risk
- **Obfuscation** - Makes code unmaintainable
- **Closed-source dependencies** - Violates open-source spirit
- **Unlicensed code** - Copyright issues

### ⚠️ Discouraged Contributions
- **Over-engineering** - SIMD, coroutines without proven need (see ROADMAP.md)
- **Breaking changes** - Without very strong justification
- **Platform-specific code** - Unless Windows-only (target platform)
- **Unnecessary dependencies** - Keep dependencies minimal

---

## 📞 Getting Help

### Communication Channels
- **GitHub Issues** - For bugs, features, questions
- **GitHub Discussions** - For general discussion (if enabled)
- **Email** - For security vulnerabilities only (see SECURITY.md)

### Response Times
- **Bug reports:** 24-48 hours
- **Feature requests:** 1-2 weeks
- **Pull requests:** 2-7 days for initial review
- **Security issues:** 24 hours

---

## 📚 Additional Resources

### Documentation
- [README.md](README.md) - Main documentation
- [ROADMAP.md](ROADMAP.md) - Future features
- [IMPLEMENTED_FEATURES.md](IMPLEMENTED_FEATURES.md) - Version history
- [docs/](docs/) - Detailed documentation

### Learning Resources
- [C++17 Reference](https://en.cppreference.com/w/cpp/17)
- [LeechCore Documentation](https://github.com/ufrisk/LeechCore)
- [MemProcFS Documentation](https://github.com/ufrisk/MemProcFS)

---

## 🙏 Thank You!

Your contributions make ArgoSentry better for everyone. Whether it's a bug report, feature request, or code contribution, every bit helps! 🚀

---

**Questions?** Open an issue or check [README.md](README.md) for contact information.
