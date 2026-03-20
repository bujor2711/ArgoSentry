# ArgoSentry - Bug Report și Fixes

## 1. 🔴 BUG CRITIC: Memory Leak în batch.cpp

**Fișier:** `src/batch.cpp` liniile 23-28  
**Severitate:** CRITIC  
**Descriere:** Utilizarea `new`/`delete` în loc de `unique_ptr` pentru gestiunea memoriei  
**Problemă:** Memory leak dacă apare o excepție între `new` și `delete`

```cpp
// ❌ Current (UNSAFE):
BatchOperations::BatchOperations()
    : pimpl_(new Impl()) {
}

BatchOperations::~BatchOperations() {
    delete pimpl_;
}
```

**Fix:** Folosire `unique_ptr` pentru RAII  
```cpp
// ✅ Fixed:
BatchOperations::BatchOperations()
    : pimpl_(std::make_unique<Impl>()) {
}

// Destructor not needed - unique_ptr handles cleanup automatically
```

---

## 2. 🔴 BUG: Null Pointer Dereference în batch.cpp

**Fișier:** `src/batch.cpp` liniile 31-32  
**Severitate:** CRITIC  
**Descriere:** Nu se verifyca dacă `pimpl_` este `nullptr` înainte de acces

```cpp
// ❌ Current:
void BatchOperations::set_vmm_handle(void* handle) {
    if (pimpl_) {
        pimpl_->vmm_handle = handle;  // OK, are check
    }
}
```

**Impact:** Dacă constructorul eșuează, `pimpl_` rămâne `nullptr`

---

## 3. 🟡 BUG: Raw Pointer Management în dma.cpp

**Fișier:** `src/dma.cpp` liniile 1150-1172, 1180-1206  
**Severitate:** MARE  
**Descriere:** Returnare raw pointer din `unique_ptr` fără protecție

```cpp
// ❌ Current (UNSAFE):
ValueFreezer* DMA::create_value_freezer(DWORD process_id) {
    auto freezer = std::make_unique<ValueFreezer>(this, process_id);
    auto* ptr = freezer.get();  // ⚠️ Raw pointer
    value_freezers_[process_id] = std::move(freezer);
    return ptr;  // ⚠️ Pointer becomes invalid if erased
}
```

**Risc:** Pointer devine invalid dacă `destroy_value_freezer()` este apelat pe alt thread  
**Fix:** Adăugare mutex protecție sau retur `shared_ptr`

---

## 4. 🟡 BUG: Thread Safety Issues în value_freezers_

**Fișier:** `src/dma.cpp` liniile 1150-1175  
**Severitate:** MARE  
**Descriere:** Race condition între `create_value_freezer()` și `destroy_value_freezer()`

```cpp
// ❌ Current:
ValueFreezer* DMA::get_value_freezer(DWORD process_id) noexcept {
    std::lock_guard<std::mutex> lock(value_freezers_mutex_);
    auto it = value_freezers_.find(process_id);
    if (it != value_freezers_.end()) {
        return it->second.get();  // Returns raw pointer while holding lock
    }                              // Lock released, pointer could be deleted
    return nullptr;
}
```

**Risc:** Use-after-free dacă alt thread sterge elementul  
**Fix:** Retur `shared_ptr` sau sincronizare mai strictă

---

## 5. 🟡 BUG: Unchecked Exception în async.cpp

**Fișier:** `src/async.cpp` liniile 215-220  
**Severitate:** MEDIE  
**Descriere:** Swallow excepții fără logging în `read_memory_range_async()`

```cpp
// ❌ Current:
try {
    for (size_t i = 0; i < size_per_address; ++i) {
        buffer[i] = dma.read<uint8_t>(addr + i, process_id);
    }
} catch (...) {
    buffer.clear();  // Silent failure - no error reporting
}
```

**Risc:** Imposibil să se știe dacă citirea a eșuat datorită hardware sau error  
**Fix:** Logging plus retur valoare care indica eroarea

---

## 6. 🔴 BUG: Potential Memory Leak în circuit_breaker init

**Fișier:** `src/dma.cpp` liniile 118-125  
**Severitate:** CRITIC  
**Descriere:** Dacă constructie `circuit_breaker_` eșuează, alte `unique_ptr` sunt deja alocate

```cpp
// ❌ Current:
metrics_ = std::make_unique<Metrics::MetricsCollector>();  // ✅ OK
cache_ = std::make_unique<Cache::MemoryCache>();           // ✅ OK
memory_analyzer_ = std::make_unique<...>();               // ✅ OK
batch_ops_->set_vmm_handle(handle_.get());                // ✅ OK
circuit_breaker_ = std::make_unique<CircuitBreaker>(...);  // ❌ If this fails?
                                                           // Previous allocations leaked?
```

**Fix:** Use try-catch în constructor

---

## 7. 🟡 BUG: Race Condition în cache.cpp

**Fișier:** `src/cache.cpp` liniile 19-35  
**Severitate:** MEDIE  
**Descriere:** Timp de Toc-Toc: shared_lock verifică, apoi shared_lock nu mai ține

```cpp
// ❌ Potential issue:
std::shared_lock<std::shared_mutex> lock(cache_mutex_);
auto it = cache_.find(key);
if (it != cache_.end()) {
    if (is_expired(it->second)) {  // ⚠️ Entry could be modified by write thread
        misses_++;
        return std::nullopt;
    }
    // ...
    return it->second.data;  // ⚠️ Could be invalid if modified
}
```

**Fix:** Upgrade lock din shared la exclusive dacă trebuie modificare

---

## 8. 🔴 BUG: Null Pointer Check Missing în dma read/write

**Fișier:** `src/dma.cpp` liniile 353-438  
**Severitate:** CRITIC  
**Descriere:** Accesare `cache_`, `memory_analyzer_` fără verificare nullptr

```cpp
// ❌ Current:
uint64_t DMA::find_signature_in_module(...) const {
    if (!memory_analyzer_) {  // ✅ Has check
        throw std::runtime_error("Memory analyzer not initialized");
    }
    // ...
}

// But in some methods:
if (cache_) {  // ❌ Check only sometimes
    auto cached = cache_->get(address, sizeof(T));
    // ...
}
```

**Fix:** Verificare consistency pentru toți pointerii

---

## 9. 🟡 BUG: Exception Safety în destructor DMA

**Fișier:** `src/dma.cpp` liniile 103-107  
**Severitate:** MEDIE  
**Descriere:** Destructor ar putea lansa excepție

```cpp
// ❌ Current:
DMA::~DMA() {
    if (health_monitor_) {
        stop_automatic_health_monitoring();  // ⚠️ Could throw
    }
    clean_fpga();  // ⚠️ Could throw
}
```

**Fix:** Adăugare `noexcept` și try-catch

---

## 10. 🟡 BUG: Incomplete Error Handling în config.cpp

**Fișier:** `src/config.cpp` liniile 29-40  
**Severitate:** MEDIE  
**Descriere:** File read/write errors nu sunt propagate, doar printate

```cpp
// ❌ Current:
if (!file.is_open()) {
    std::cerr << "[Config] Failed to open config file: " << filepath << std::endl;
    return false;  // ⚠️ Caller doesn't know why it failed
}
```

**Fix:** Return meaningful error codes sau throw exceptions

---

## Summary of Critical Bugs

| Bug | File | Type | Fix |
|-----|------|------|-----|
| 1 | batch.cpp | Memory Leak | Use `unique_ptr` |
| 2 | dma.cpp | Raw Pointer | Protect with mutex or use `shared_ptr` |
| 3 | dma.cpp | Thread Safety | Synchronize access to freezers |
| 4 | async.cpp | Silent Failure | Add error logging |
| 5 | dma.cpp | Constructor Safety | Add exception handling |
| 6 | cache.cpp | Race Condition | Upgrade lock to exclusive |
| 7 | dma.cpp | Null Check | Add checks to all methods |
| 8 | dma.cpp | Exception Safety | Add noexcept to destructor |
| 9 | config.cpp | Error Handling | Return meaningful errors |

