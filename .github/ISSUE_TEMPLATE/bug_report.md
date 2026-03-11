---
name: 🐛 Bug Report
about: Report a bug to help improve ArgoSentry
title: '[BUG] '
labels: bug
assignees: ''
---

## 🐛 Bug Description
<!-- A clear and concise description of what the bug is -->

## 🔄 Steps to Reproduce
1. Initialize DMA with '...'
2. Call method '....'
3. Observe error '....'

## ✅ Expected Behavior
<!-- What you expected to happen -->

## ❌ Actual Behavior
<!-- What actually happened -->

## 💻 Environment
- **OS:** [e.g., Windows 11 22H2]
- **Compiler:** [e.g., MSVC 2026 (v19.45)]
- **ArgoSentry Version:** [e.g., v2.3.0]
- **DMA Hardware:** [e.g., Squirrel DMA, 35T FPGA]
- **Build Configuration:** [Debug/Release]

## 📝 Code Snippet
```cpp
// Minimal reproducible example
#include <ArgoSentry/dma.hh>
using namespace ArgoSentry;

int main() {
    auto dma = DMA::Builder()
        .with_cache(100 * 1024 * 1024)
        .build();
    
    // Your code that triggers the bug...
    
    return 0;
}
```

## 📊 Error Messages / Logs
```
Paste any error messages, stack traces, or relevant log output here
```

## 🔍 Additional Context
<!-- Any other relevant information, screenshots, or context -->

## ✅ Checklist
- [ ] I have searched existing issues to ensure this is not a duplicate
- [ ] I have tested with the latest version (v2.3)
- [ ] I can reproduce this consistently
- [ ] I have included a minimal reproducible example
- [ ] I have checked that DMA hardware is properly connected

## 💡 Possible Solution
<!-- If you have ideas on how to fix this, describe them here -->
