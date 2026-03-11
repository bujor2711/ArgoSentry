// ArgoSentry/compiled_pattern.cpp - Implementation
// v2.5 - Pre-compiled pattern optimization

#include "ArgoSentry/compiled_pattern.hh"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace ArgoSentry {

//==============================================================================
// Static Helper Methods
//==============================================================================

bool CompiledPattern::is_hex_digit(char c) noexcept {
    return (c >= '0' && c <= '9') || 
           (c >= 'A' && c <= 'F') || 
           (c >= 'a' && c <= 'f');
}

uint8_t CompiledPattern::hex_char_to_value(char c) noexcept {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
    return 0;
}

bool CompiledPattern::parse_token(const std::string& token, uint8_t& out_byte, uint8_t& out_mask) {
    // Empty token
    if (token.empty()) {
        return false;
    }

    // Check for wildcard
    if (token == "?" || token == "??") {
        out_byte = 0x00;
        out_mask = 0x00;  // Wildcard
        return true;
    }

    // Must be hex byte (1 or 2 characters)
    if (token.length() > 2) {
        return false;
    }

    // Validate all characters are hex
    for (char c : token) {
        if (!is_hex_digit(c)) {
            return false;
        }
    }

    // Parse hex value
    try {
        // Convert to uppercase for consistency
        std::string upper_token = token;
        std::transform(upper_token.begin(), upper_token.end(), upper_token.begin(), ::toupper);

        // Parse hex string
        size_t pos = 0;
        unsigned long value = std::stoul(upper_token, &pos, 16);

        if (pos != upper_token.length() || value > 0xFF) {
            return false;
        }

        out_byte = static_cast<uint8_t>(value);
        out_mask = 0xFF;  // Exact match
        return true;

    } catch (...) {
        return false;
    }
}

//==============================================================================
// Public Methods
//==============================================================================

CompiledPattern CompiledPattern::compile(const std::string& signature) {
    CompiledPattern result;

    // Trim signature
    std::string trimmed = signature;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        throw std::invalid_argument("Empty signature pattern");
    }

    // Parse tokens
    std::istringstream stream(trimmed);
    std::string token;

    while (stream >> token) {
        uint8_t byte_value = 0;
        uint8_t mask_value = 0;

        if (!parse_token(token, byte_value, mask_value)) {
            throw std::invalid_argument(
                "Invalid token in signature: '" + token + "'. " +
                "Expected hex byte (00-FF) or wildcard (?)"
            );
        }

        result.bytes_.push_back(byte_value);
        result.mask_.push_back(mask_value);
    }

    result.length_ = result.bytes_.size();

    if (result.length_ == 0) {
        throw std::invalid_argument("Pattern compilation resulted in zero length");
    }

    // Validate reasonable size (prevent abuse)
    if (result.length_ > 1024) {
        throw std::invalid_argument(
            "Pattern too long (" + std::to_string(result.length_) + " bytes). " +
            "Maximum is 1024 bytes."
        );
    }

    return result;
}

uint64_t CompiledPattern::find_in_buffer(
    const uint8_t* data, 
    size_t size, 
    uint64_t base_addr
) const {
    // Validate inputs - MUST be done BEFORE any arithmetic
    if (!data || size == 0 || length_ == 0) {
        return 0;
    }

    // ✅ Prevent unsigned underflow: Check size >= length_ before arithmetic
    if (size < length_) {
        return 0;  // Buffer too small
    }

    // Calculate search range (safe now - size >= length_ guaranteed)
    const size_t search_end = size - length_ + 1;

    // Fast scanning with mask
    for (size_t i = 0; i < search_end; ++i) {
        bool match = true;

        // Check all bytes with mask
        for (size_t j = 0; j < length_; ++j) {
            if (mask_[j] == 0xFF) {  // Exact match required
                if (data[i + j] != bytes_[j]) {
                    match = false;
                    break;  // Early exit on mismatch
                }
            }
            // mask_[j] == 0x00: Wildcard - always matches
        }

        if (match) {
            return base_addr + i;  // Found!
        }
    }

    return 0;  // Not found
}

std::string CompiledPattern::to_string() const {
    if (length_ == 0) {
        return "(empty pattern)";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < length_; ++i) {
        if (i > 0) {
            oss << " ";
        }

        if (mask_[i] == 0x00) {
            oss << "??";  // Wildcard
        } else {
            oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                << static_cast<int>(bytes_[i]);
        }
    }

    return oss.str();
}

} // namespace ArgoSentry
