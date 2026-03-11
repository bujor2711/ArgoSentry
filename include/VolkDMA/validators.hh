#pragma once

#include <cstdint>
#include <string>
#include <optional>

using DWORD = unsigned long;

namespace VolkDMA {
namespace Validation {

// Validator pentru signature patterns
class SignatureValidator {
public:
    // Verifică dacă pattern-ul este valid (format: "E8 ? ? ? ? 48" sau "E8??????48")
    static bool is_valid_hex_pattern(const char* signature);
    
    // Returnează lungimea pattern-ului în bytes (nu în caractere)
    static size_t get_pattern_length(const char* signature);
    
    // Verifică dacă un byte hex individual este valid (ex: "E8", "?")
    static bool is_valid_hex_byte(const char* hex);
    
    // Verifică dacă un caracter este hex valid (0-9, A-F, a-f)
    static bool is_hex_char(char c);
    
    // Normalizează pattern-ul (adaugă spații dacă lipsesc)
    static std::string normalize_pattern(const char* signature);
};

// Validator pentru memory ranges
class MemoryRangeValidator {
public:
    // Verifică dacă range-ul este valid (start < end, nu overflow)
    static bool is_safe_range(uint64_t start, uint64_t end);
    
    // Verifică dacă start + size ar produce overflow
    static bool would_overflow(uint64_t start, uint64_t size);
    
    // Limitează size-ul la o valoare sigură (max 2GB)
    static uint64_t clamp_to_safe_size(uint64_t size);
    
    // Verifică dacă adresa este aliniată la page boundary (opțional)
    static bool is_page_aligned(uint64_t address);
    
    // Constante pentru validare
    static constexpr uint64_t MAX_SAFE_SIZE = 2ULL * 1024 * 1024 * 1024; // 2GB
    static constexpr uint64_t PAGE_SIZE = 4096;
};

// Validator pentru process IDs
class ProcessValidator {
public:
    // Verifică dacă PID-ul este valid (> 0, nu este PID de sistem)
    static bool is_valid_process_id(DWORD pid);
    
    // Verifică dacă PID-ul este un process de sistem care nu trebuie accesat
    static bool is_system_process(DWORD pid);
    
    // Constante pentru validare
    static constexpr DWORD SYSTEM_IDLE_PROCESS_ID = 0;
    static constexpr DWORD SYSTEM_PROCESS_ID = 4;
};

// Result type pentru validări cu erori detaliate
template<typename T>
struct ValidationResult {
    bool valid;
    T value;
    std::string error_message;
    
    static ValidationResult<T> success(T val) {
        return {true, val, ""};
    }
    
    static ValidationResult<T> failure(const std::string& error) {
        return {false, T{}, error};
    }
    
    explicit operator bool() const { return valid; }
};

} // namespace Validation
} // namespace VolkDMA
