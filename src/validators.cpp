#include "include/VolkDMA/validators.hh"

#include <cctype>
#include <algorithm>
#include <sstream>

namespace VolkDMA {
namespace Validation {

// ============================================================================
// SignatureValidator Implementation
// ============================================================================

bool SignatureValidator::is_hex_char(char c) {
    return (c >= '0' && c <= '9') || 
           (c >= 'A' && c <= 'F') || 
           (c >= 'a' && c <= 'f');
}

bool SignatureValidator::is_valid_hex_byte(const char* hex) {
    if (!hex) return false;
    
    // Wildcard
    if (hex[0] == '?') {
        return hex[1] == '\0' || hex[1] == ' ' || hex[1] == '?';
    }
    
    // Must be exactly 2 hex characters
    if (!is_hex_char(hex[0]) || !is_hex_char(hex[1])) {
        return false;
    }
    
    // Third character must be space, null, or another hex digit (for no-space format)
    char third = hex[2];
    return third == '\0' || third == ' ' || is_hex_char(third);
}

bool SignatureValidator::is_valid_hex_pattern(const char* signature) {
    if (!signature || signature[0] == '\0') {
        return false;
    }
    
    const char* ptr = signature;
    bool found_at_least_one_byte = false;
    
    while (*ptr != '\0') {
        // Skip spaces
        while (*ptr == ' ') {
            ptr++;
        }
        
        if (*ptr == '\0') break;
        
        // Check for wildcard
        if (*ptr == '?') {
            found_at_least_one_byte = true;
            ptr++;
            // Handle "??" format
            if (*ptr == '?') {
                ptr++;
            }
            // Skip optional space after wildcard
            if (*ptr == ' ') {
                ptr++;
            }
            continue;
        }
        
        // Must be a hex byte
        if (!is_hex_char(*ptr)) {
            return false;
        }
        
        // Must have a second hex character
        if (!is_hex_char(*(ptr + 1))) {
            return false;
        }
        
        found_at_least_one_byte = true;
        ptr += 2;
        
        // Skip optional space after byte
        if (*ptr == ' ') {
            ptr++;
        }
    }
    
    return found_at_least_one_byte;
}

size_t SignatureValidator::get_pattern_length(const char* signature) {
    if (!is_valid_hex_pattern(signature)) {
        return 0;
    }
    
    const char* ptr = signature;
    size_t length = 0;
    
    while (*ptr != '\0') {
        // Skip spaces
        while (*ptr == ' ') {
            ptr++;
        }
        
        if (*ptr == '\0') break;
        
        // Wildcard or hex byte = 1 byte in pattern
        if (*ptr == '?') {
            length++;
            ptr++;
            if (*ptr == '?') ptr++; // Handle "??"
            if (*ptr == ' ') ptr++;
        } else if (is_hex_char(*ptr)) {
            length++;
            ptr += 2; // Skip two hex chars
            if (*ptr == ' ') ptr++;
        }
    }
    
    return length;
}

std::string SignatureValidator::normalize_pattern(const char* signature) {
    if (!is_valid_hex_pattern(signature)) {
        return "";
    }
    
    std::ostringstream result;
    const char* ptr = signature;
    bool first = true;
    
    while (*ptr != '\0') {
        // Skip spaces
        while (*ptr == ' ') {
            ptr++;
        }
        
        if (*ptr == '\0') break;
        
        if (!first) {
            result << ' ';
        }
        first = false;
        
        if (*ptr == '?') {
            result << '?';
            ptr++;
            if (*ptr == '?') ptr++; // Skip second '?'
        } else if (is_hex_char(*ptr)) {
            result << *ptr;
            result << *(ptr + 1);
            ptr += 2;
        }
        
        // Skip optional space
        if (*ptr == ' ') {
            ptr++;
        }
    }
    
    return result.str();
}

// ============================================================================
// MemoryRangeValidator Implementation
// ============================================================================

bool MemoryRangeValidator::is_safe_range(uint64_t start, uint64_t end) {
    // Check basic validity
    if (start >= end) {
        return false;
    }
    
    // Check for overflow when calculating size
    uint64_t size = end - start;
    if (size > MAX_SAFE_SIZE) {
        return false;
    }
    
    // Check if range would wrap around address space
    if (end < start) {
        return false;
    }
    
    return true;
}

bool MemoryRangeValidator::would_overflow(uint64_t start, uint64_t size) {
    // Check if start + size would overflow
    if (size > (UINT64_MAX - start)) {
        return true;
    }
    
    // Check if size is unreasonably large
    if (size > MAX_SAFE_SIZE) {
        return true;
    }
    
    return false;
}

uint64_t MemoryRangeValidator::clamp_to_safe_size(uint64_t size) {
    return (size > MAX_SAFE_SIZE) ? MAX_SAFE_SIZE : size;
}

bool MemoryRangeValidator::is_page_aligned(uint64_t address) {
    return (address % PAGE_SIZE) == 0;
}

// ============================================================================
// ProcessValidator Implementation
// ============================================================================

bool ProcessValidator::is_valid_process_id(DWORD pid) {
    // PID must be greater than 0
    if (pid == 0) {
        return false;
    }
    
    // Don't allow accessing system processes
    if (is_system_process(pid)) {
        return false;
    }
    
    return true;
}

bool ProcessValidator::is_system_process(DWORD pid) {
    // System Idle Process (PID 0)
    if (pid == SYSTEM_IDLE_PROCESS_ID) {
        return true;
    }
    
    // System Process (PID 4)
    if (pid == SYSTEM_PROCESS_ID) {
        return true;
    }
    
    return false;
}

} // namespace Validation
} // namespace VolkDMA
