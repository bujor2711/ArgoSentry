# Security Policy

## 🔒 Supported Versions

We actively maintain and provide security updates for the following versions:

| Version | Supported          | Status |
| ------- | ------------------ | ------ |
| 2.3.x   | ✅ Yes | Current stable release |
| 2.2.x   | ✅ Yes | Security fixes only |
| 2.1.x   | ⚠️ Limited | Critical security fixes only |
| 2.0.x   | ❌ No | End of life - upgrade recommended |
| < 2.0   | ❌ No | End of life - upgrade required |

**Recommendation:** Always use the latest stable version (v2.3.x) for best security and features.

---

## 🐛 Reporting a Vulnerability

### ⚠️ CRITICAL: Do NOT open public issues for security vulnerabilities!

Security vulnerabilities could be exploited if disclosed publicly before a fix is available.

### Reporting Channels

**Option 1: GitHub Security Advisory (Recommended)**
1. Go to: https://github.com/bujor2711/ArgoSentry/security/advisories
2. Click "Report a vulnerability"
3. Fill in the details (see template below)

**Option 2: Private Email**
- **Email:** [To be added - maintainer's security contact]
- **Subject:** `[SECURITY] ArgoSentry - <Brief Description>`
- **Encryption:** PGP key available upon request

### What to Include in Your Report

Please provide as much information as possible:

```markdown
**Summary:**
Brief description of the vulnerability

**Severity:** [Critical / High / Medium / Low]

**Affected Versions:**
- ArgoSentry versions: [e.g., v2.0 - v2.3]

**Description:**
Detailed explanation of the vulnerability

**Steps to Reproduce:**
1. Initialize DMA with...
2. Call function X with...
3. Observe vulnerability...

**Proof of Concept (PoC):**
```cpp
// Minimal code to demonstrate the issue
#include <ArgoSentry/dma.hh>

int main() {
    // PoC code here
    return 0;
}
```

**Impact:**
- What could an attacker do with this vulnerability?
- How severe is the impact?
- What systems/data are at risk?

**Suggested Fix (if known):**
- How could this be fixed?
- Are there any workarounds?

**References:**
- CVE IDs (if applicable)
- Related vulnerabilities
- Research papers
```

---

## 🕐 Response Timeline

We take security seriously and will respond promptly:

| Timeframe | Action |
|-----------|--------|
| **Within 24 hours** | Initial acknowledgment of report |
| **Within 48 hours** | Preliminary assessment and severity classification |
| **Within 7 days** | Detailed analysis and proposed fix timeline |
| **Within 30-90 days** | Fix developed, tested, and released (depending on severity) |

### Severity Classification

**Critical (90-day maximum):**
- Remote code execution
- Arbitrary memory write without authorization
- Privilege escalation
- Data exfiltration

**High (60-day maximum):**
- Denial of Service (DoS) vulnerabilities
- Memory corruption bugs
- Authentication bypass

**Medium (90-day maximum):**
- Information disclosure
- Race conditions
- Resource leaks

**Low (Best effort):**
- Minor information leaks
- Deprecation warnings
- Non-security bugs

---

## 🔐 Security Considerations for Users

### Known Security Risks

ArgoSentry is a **Direct Memory Access (DMA)** library that requires elevated privileges and hardware access. Be aware of these inherent risks:

#### ⚠️ Elevated Privileges Required
- **Risk:** ArgoSentry requires administrator/kernel-level access
- **Mitigation:** Only run trusted code, audit dependencies
- **Best Practice:** Use virtualization or dedicated hardware for testing

#### ⚠️ Hardware DMA Access
- **Risk:** Direct memory access bypasses OS security
- **Mitigation:** Understand what you're reading/writing
- **Best Practice:** Use read-only mode whenever possible

#### ⚠️ Terms of Service Violations
- **Risk:** Using DMA for game hacking violates most ToS
- **Mitigation:** Understand legal implications
- **Best Practice:** Only use for authorized security research

#### ⚠️ Anti-Cheat Detection
- **Risk:** Anti-cheat systems may detect DMA usage
- **Mitigation:** Rate limiting (v2.3) helps reduce detection
- **Best Practice:** Don't use in production game environments

### Recommended Security Practices

#### 1. **Validate All Inputs**
```cpp
// ✅ DO: Validate addresses before reading
if (address < MIN_VALID_ADDRESS || address > MAX_VALID_ADDRESS) {
    throw std::invalid_argument("Invalid address");
}

// ❌ DON'T: Blindly trust user input
auto value = dma->read_u64(user_input_address, pid);  // Dangerous!
```

#### 2. **Use Rate Limiting (v2.3)**
```cpp
// ✅ DO: Enable rate limiting to avoid detection
auto dma = DMA::Builder()
    .with_rate_limit(1 * 1024 * 1024)  // 1 MB/s
    .build();
```

#### 3. **Error Handling**
```cpp
// ✅ DO: Handle errors gracefully
try {
    auto value = dma->read_u64(address, pid);
} catch (const std::exception& e) {
    // Log error, don't crash
    std::cerr << "Read failed: " << e.what() << "\n";
}
```

#### 4. **Memory Safety**
```cpp
// ✅ DO: Use cache to reduce hardware access
auto dma = DMA::Builder()
    .with_cache(100 * 1024 * 1024)  // 100MB cache
    .build();

// ✅ DO: Validate buffer sizes
if (buffer.size() < expected_size) {
    throw std::runtime_error("Buffer too small");
}
```

#### 5. **Audit Dependencies**
```cpp
// ⚠️ WARNING: These DLLs are required but have elevated privileges
// - vmm.dll (patched version included)
// - leechcore.dll
// - FTD3XX.dll (FTDI driver)
//
// Only use trusted builds from official sources!
```

---

## 🛡️ Security Features in ArgoSentry

### v2.3 Security Improvements
- ✅ **Rate Limiting** - Prevents detection via behavioral analysis
- ✅ **Thread-safe operations** - Prevents race conditions
- ✅ **Memory bounds checking** - Validates addresses
- ✅ **Error handling** - Graceful failure instead of crashes

### v2.2 Security Features
- ✅ **Builder pattern** - Safer initialization
- ✅ **RAII** - Automatic resource cleanup

### v2.1 Security Features
- ✅ **Memory diffing** - Detect unauthorized changes
- ✅ **Cache TTL** - Prevents stale data

### v1.0+ Security Features
- ✅ **Input validation** - Rejects invalid addresses
- ✅ **Exception handling** - Controlled error propagation
- ✅ **Resource management** - RAII for DMA handles

---

## 🔍 Security Audits

### Latest Audit
- **Date:** Not yet conducted
- **Status:** Planning security audit for v2.4

### Request an Audit
If you're a security researcher interested in auditing ArgoSentry:
1. Contact maintainer via GitHub
2. Provide credentials and scope
3. We'll provide test environment access

---

## 🏆 Security Researchers Hall of Fame

We appreciate responsible security researchers who help make ArgoSentry safer:

<!-- 
**v2.3:**
- [Name] - Reported [vulnerability] - [Date]

**v2.2:**
- [Name] - Reported [vulnerability] - [Date]
-->

*Be the first to contribute to our security!*

---

## 🔐 Responsible Disclosure Policy

We follow a **90-day disclosure timeline**:

1. **Day 0:** Vulnerability reported
2. **Day 7:** Severity assessed, fix timeline provided
3. **Day 30-90:** Fix developed and released (depending on severity)
4. **Day 90+:** Public disclosure (coordinated with reporter)

### Disclosure Credits

After disclosure, we will:
- ✅ Credit reporter in CHANGELOG.md (if desired)
- ✅ Credit reporter in GitHub Security Advisory
- ✅ Mention in release notes
- ✅ Add to Security Hall of Fame

### Exceptions

We may request **extended embargo** for:
- Critical vulnerabilities affecting many users
- Vulnerabilities with active exploitation in the wild
- Vulnerabilities requiring coordinated disclosure with dependencies

---

## 📚 Additional Security Resources

### External Documentation
- [OWASP Secure Coding Practices](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
- [CWE Top 25 Most Dangerous Software Errors](https://cwe.mitre.org/top25/)
- [Microsoft Security Development Lifecycle](https://www.microsoft.com/en-us/securityengineering/sdl)

### ArgoSentry Documentation
- [CONTRIBUTING.md](CONTRIBUTING.md) - Secure development practices
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) - Community standards
- [ROADMAP.md](ROADMAP.md) - Future security features

---

## ⚖️ Legal Disclaimer

ArgoSentry is provided "as is" without warranty of any kind. Users are responsible for:
- Complying with local laws and regulations
- Respecting Terms of Service of target applications
- Understanding security implications
- Taking responsibility for their actions

**Use at your own risk.** The maintainers are not responsible for misuse.

---

## 📞 Contact

For security-related inquiries:
- **GitHub:** Use Security Advisory feature
- **Email:** [To be added]
- **PGP Key:** Available upon request

For non-security issues, use regular GitHub Issues.

---

**Last Updated:** March 2026  
**Version:** 1.0  
**Next Review:** June 2026
