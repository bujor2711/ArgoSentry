// pattern_scanner_enhanced.cpp - Enhanced Pattern Scanner Implementation
// IDA-style pattern scanning with wildcards

#include "../include/ArgoSentry/pattern_scanner_enhanced.hh"
#include "../include/ArgoSentry/dma.hh"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <chrono>

namespace ArgoSentry {

EnhancedPatternScanner::EnhancedPatternScanner(DMA* dma, DWORD process_id)
    : dma_(dma)
    , process_id_(process_id)
{
    if (!dma_) {
        throw std::invalid_argument("DMA instance cannot be null");
    }
}

std::vector<uint64_t> EnhancedPatternScanner::scan_pattern(
    const std::string& pattern,
    uint64_t start_address,
    uint64_t end_address,
    bool first_match_only
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Check cache first
    std::string cache_key = generate_cache_key(pattern);
    CompiledPattern compiled;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pattern_cache_.find(cache_key);
        if (it != pattern_cache_.end()) {
            compiled = it->second;
        } else {
            compiled = parse_ida_pattern(pattern);
            pattern_cache_[cache_key] = compiled;
            stats_.cached_patterns = pattern_cache_.size();
        }
    }
    
    // Scan with compiled pattern
    auto results = scan_compiled(compiled, start_address, end_address, first_match_only);
    
    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.total_scans++;
        stats_.total_matches += results.size();
        stats_.average_scan_time_ms = 
            (stats_.average_scan_time_ms * (stats_.total_scans - 1) + duration) / stats_.total_scans;
    }
    
    return results;
}

std::vector<PatternMatch> EnhancedPatternScanner::scan_multi_patterns(
    const std::vector<std::string>& patterns,
    uint64_t start_address,
    uint64_t end_address,
    bool first_match_only
) {
    std::vector<PatternMatch> all_matches;
    
    // Compile all patterns
    std::vector<std::pair<std::string, CompiledPattern>> compiled_patterns;
    for (size_t i = 0; i < patterns.size(); ++i) {
        std::string pattern_id = "pattern_" + std::to_string(i);
        auto compiled = compile_pattern(patterns[i], pattern_id);
        compiled_patterns.push_back({pattern_id, compiled});
    }
    
    // Scan with each pattern
    for (const auto& [pattern_id, compiled] : compiled_patterns) {
        auto matches = scan_compiled(compiled, start_address, end_address, first_match_only);
        
        for (size_t i = 0; i < matches.size(); ++i) {
            all_matches.emplace_back(matches[i], pattern_id, i);
        }
        
        // Stop if first match found
        if (first_match_only && !all_matches.empty()) {
            break;
        }
    }
    
    // Sort by address
    std::sort(all_matches.begin(), all_matches.end(),
        [](const PatternMatch& a, const PatternMatch& b) {
            return a.address < b.address;
        });
    
    return all_matches;
}

CompiledPattern EnhancedPatternScanner::compile_pattern(
    const std::string& pattern,
    const std::string& pattern_id
) {
    std::string cache_key = pattern_id.empty() ? generate_cache_key(pattern) : pattern_id;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already cached
    auto it = pattern_cache_.find(cache_key);
    if (it != pattern_cache_.end()) {
        return it->second;
    }
    
    // Parse and cache
    auto compiled = parse_ida_pattern(pattern);
    pattern_cache_[cache_key] = compiled;
    stats_.cached_patterns = pattern_cache_.size();
    
    return compiled;
}

std::vector<uint64_t> EnhancedPatternScanner::scan_compiled(
    const CompiledPattern& compiled,
    uint64_t start_address,
    uint64_t end_address,
    bool first_match_only
) {
    std::vector<uint64_t> results;
    
    if (end_address <= start_address || compiled.pattern_size == 0) {
        return results;
    }
    
    // Read memory in chunks (1MB at a time)
    const size_t chunk_size = 1024 * 1024;
    uint64_t current_address = start_address;
    
    while (current_address < end_address) {
        size_t read_size = std::min(chunk_size, static_cast<size_t>(end_address - current_address));
        
        // Read chunk
        std::vector<uint8_t> buffer(read_size);
        auto read_result = dma_->read_raw(current_address, buffer.data(), read_size, process_id_);
        
        if (!read_result.has_value() || read_result.value() == 0) {
            current_address += read_size;
            continue;
        }
        
        size_t bytes_read = read_result.value();
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.bytes_scanned += bytes_read;
        }
        
        // Find matches in buffer
        std::vector<size_t> match_offsets;
        match_pattern_in_buffer(buffer.data(), bytes_read, compiled, match_offsets, first_match_only);
        
        // Convert offsets to addresses
        for (size_t offset : match_offsets) {
            results.push_back(current_address + offset);
            
            if (first_match_only) {
                return results;
            }
        }
        
        // Move to next chunk (with overlap for patterns spanning chunks)
        current_address += read_size;
        if (compiled.pattern_size > 1) {
            current_address -= (compiled.pattern_size - 1);
        }
    }
    
    return results;
}

void EnhancedPatternScanner::clear_cache() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    pattern_cache_.clear();
    stats_.cached_patterns = 0;
}

void EnhancedPatternScanner::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = PatternScanStats{};
}

bool EnhancedPatternScanner::is_cached(const std::string& pattern_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pattern_cache_.find(pattern_id) != pattern_cache_.end();
}

std::optional<CompiledPattern> EnhancedPatternScanner::get_cached_pattern(
    const std::string& pattern_id
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pattern_cache_.find(pattern_id);
    if (it != pattern_cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

CompiledPattern EnhancedPatternScanner::parse_ida_pattern(const std::string& pattern) {
    CompiledPattern compiled;
    compiled.original = pattern;
    
    std::string cleaned;
    
    // Remove spaces and convert to uppercase
    for (char c : pattern) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            cleaned += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    
    // Parse hex bytes and wildcards
    for (size_t i = 0; i < cleaned.length(); i += 2) {
        if (i + 1 >= cleaned.length()) {
            break; // Incomplete byte
        }
        
        std::string byte_str = cleaned.substr(i, 2);
        
        // Check for wildcard
        if (byte_str == "??") {
            compiled.bytes.push_back(0x00); // Placeholder
            compiled.mask.push_back(false); // Wildcard
        } else {
            // Parse hex byte
            try {
                uint8_t byte_val = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                compiled.bytes.push_back(byte_val);
                compiled.mask.push_back(true); // Match
            } catch (...) {
                // Invalid hex, treat as wildcard
                compiled.bytes.push_back(0x00);
                compiled.mask.push_back(false);
            }
        }
    }
    
    compiled.pattern_size = compiled.bytes.size();
    return compiled;
}

void EnhancedPatternScanner::match_pattern_in_buffer(
    const uint8_t* buffer,
    size_t buffer_size,
    const CompiledPattern& compiled,
    std::vector<size_t>& matches,
    bool first_match_only
) const {
    if (buffer_size < compiled.pattern_size) {
        return;
    }
    
    // Simple pattern matching with mask
    for (size_t i = 0; i <= buffer_size - compiled.pattern_size; ++i) {
        bool match = true;
        
        // Check each byte in pattern
        for (size_t j = 0; j < compiled.pattern_size; ++j) {
            // Skip wildcards
            if (!compiled.mask[j]) {
                continue;
            }
            
            // Check byte match
            if (buffer[i + j] != compiled.bytes[j]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            matches.push_back(i);
            
            if (first_match_only) {
                return;
            }
        }
    }
}

std::string EnhancedPatternScanner::generate_cache_key(const std::string& pattern) const {
    // Remove spaces and convert to uppercase for consistent caching
    std::string key;
    for (char c : pattern) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            key += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return key;
}

} // namespace ArgoSentry
