#include "ArgoSentry/mock_dma.hh"
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iomanip>

namespace ArgoSentry {

// ============================================================================
// Helper Functions
// ============================================================================

bool MockDMA::validate_address(uint64_t addr) const {
    return addr >= MIN_VALID_ADDRESS && addr <= MAX_VALID_ADDRESS;
}

std::optional<std::reference_wrapper<MockDMA::MemoryRegion>>
MockDMA::find_region(uint64_t address) {
    for (auto& [base, region] : memory_regions_) {
        if (address >= region.base_address &&
            address < region.base_address + region.data.size()) {
            region.last_access = std::chrono::steady_clock::now();
            return std::ref(region);
        }
    }
    return std::nullopt;
}

std::vector<MockDMA::PatternByte> MockDMA::parse_pattern(const char* pattern) const {
    std::vector<PatternByte> result;
    std::istringstream stream(pattern);
    std::string token;

    while (stream >> token) {
        PatternByte byte;

        if (token == "?" || token == "??") {
            byte.value = 0x00;
            byte.is_wildcard = true;
        }
        else {
            // Parse hex byte
            try {
                size_t pos;
                int value = std::stoi(token, &pos, 16);

                if (pos != token.size() || value < 0 || value > 0xFF) {
                    throw std::invalid_argument("Invalid hex byte in pattern: " + token);
                }

                byte.value = static_cast<uint8_t>(value);
                byte.is_wildcard = false;
            }
            catch (const std::exception&) {
                throw std::invalid_argument("Failed to parse pattern byte: " + token);
            }
        }

        result.push_back(byte);
    }

    if (result.empty()) {
        throw std::invalid_argument("Empty pattern");
    }

    return result;
}

bool MockDMA::match_pattern(const uint8_t* data, size_t size,
                            const std::vector<PatternByte>& pattern) const {
    if (size < pattern.size()) {
        return false;
    }

    for (size_t i = 0; i < pattern.size(); ++i) {
        if (!pattern[i].is_wildcard && data[i] != pattern[i].value) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Public Methods
// ============================================================================

void MockDMA::set_memory(uint64_t addr, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!validate_address(addr)) {
        throw std::invalid_argument(
            "Invalid address: 0x" + std::to_string(addr) +
            " (must be between 0x" + std::to_string(MIN_VALID_ADDRESS) +
            " and 0x" + std::to_string(MAX_VALID_ADDRESS) + ")"
        );
    }

    if (data.empty()) {
        throw std::invalid_argument("Cannot set empty memory region");
    }

    // Check if region already exists
    auto it = memory_regions_.find(addr);
    if (it != memory_regions_.end()) {
        total_memory_used_ -= it->second.data.size();
    }

    // Check memory limit
    if (total_memory_used_ + data.size() > MAX_MEMORY_SIZE) {
        throw std::runtime_error(
            "Mock memory limit exceeded. Used: " +
            std::to_string(total_memory_used_) +
            " bytes, limit: " +
            std::to_string(MAX_MEMORY_SIZE) + " bytes"
        );
    }

    // Store region
    MemoryRegion region;
    region.data = data;
    region.base_address = addr;
    region.last_access = std::chrono::steady_clock::now();

    memory_regions_[addr] = std::move(region);
    total_memory_used_ += data.size();
}

void MockDMA::set_process(const std::string& name, DWORD pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    mock_processes_[name] = pid;
}

void MockDMA::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_regions_.clear();
    mock_processes_.clear();
    total_memory_used_ = 0;
    stats_.reset();
}

void MockDMA::evict_oldest_region() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (memory_regions_.empty()) {
        return;
    }

    auto oldest = memory_regions_.begin();
    auto oldest_time = oldest->second.last_access;

    for (auto it = memory_regions_.begin(); it != memory_regions_.end(); ++it) {
        if (it->second.last_access < oldest_time) {
            oldest = it;
            oldest_time = it->second.last_access;
        }
    }

    total_memory_used_ -= oldest->second.data.size();
    memory_regions_.erase(oldest);
    stats_.evictions++;
}

const MockDMA::Statistics& MockDMA::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

size_t MockDMA::get_memory_usage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_memory_used_;
}

size_t MockDMA::get_region_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return memory_regions_.size();
}

// ============================================================================
// IDMAInterface Implementation
// ============================================================================

uint8_t MockDMA::read_u8(uint64_t address, DWORD pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.read_count++;

    auto region = find_region(address);
    if (!region.has_value()) {
        stats_.cache_misses++;
        throw std::runtime_error(
            "Address not in mock memory: 0x" + std::to_string(address)
        );
    }

    stats_.cache_hits++;
    size_t offset = address - region->get().base_address;
    return region->get().data[offset];
}

uint16_t MockDMA::read_u16(uint64_t address, DWORD pid) {
    uint16_t result = 0;
    result |= read_u8(address, pid);
    result |= static_cast<uint16_t>(read_u8(address + 1, pid)) << 8;
    return result;
}

uint32_t MockDMA::read_u32(uint64_t address, DWORD pid) {
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        result |= static_cast<uint32_t>(read_u8(address + i, pid)) << (i * 8);
    }
    return result;
}

uint64_t MockDMA::read_u64(uint64_t address, DWORD pid) {
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result |= static_cast<uint64_t>(read_u8(address + i, pid)) << (i * 8);
    }
    return result;
}

std::vector<uint8_t> MockDMA::read_bytes(uint64_t address, size_t size, DWORD pid) {
    std::vector<uint8_t> result;
    result.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        result.push_back(read_u8(address + i, pid));
    }
    return result;
}

uint64_t MockDMA::find_signature(const char* pattern, uint64_t start,
                                 uint64_t end, DWORD pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.find_count++;

    if (!pattern) {
        throw std::invalid_argument("Pattern is null");
    }

    if (end <= start) {
        throw std::invalid_argument(
            "Invalid range: end <= start (0x" +
            std::to_string(end) + " <= 0x" + std::to_string(start) + ")"
        );
    }

    // Parse pattern
    std::vector<PatternByte> pattern_bytes;
    try {
        pattern_bytes = parse_pattern(pattern);
    }
    catch (const std::exception& e) {
        throw std::invalid_argument(
            std::string("Pattern parse error: ") + e.what()
        );
    }

    // Search in all regions that overlap the range
    for (auto& [base, region] : memory_regions_) {
        uint64_t region_start = region.base_address;
        uint64_t region_end = region.base_address + region.data.size();

        // Check if region overlaps with search range
        if (region_end <= start || region_start >= end) {
            continue;  // No overlap
        }

        // Calculate search bounds within this region
        size_t search_start_offset = (start > region_start) ? (start - region_start) : 0;
        size_t search_end_offset = (end < region_end) ? (end - region_start) : region.data.size();

        // Search for pattern
        for (size_t i = search_start_offset; i < search_end_offset; ++i) {
            if (i + pattern_bytes.size() > region.data.size()) {
                break;  // Pattern would extend beyond region
            }

            if (match_pattern(region.data.data() + i, region.data.size() - i, pattern_bytes)) {
                region.last_access = std::chrono::steady_clock::now();
                return region.base_address + i;
            }
        }
    }

    return 0;  // Not found
}

DWORD MockDMA::get_process_id(const char* name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!name) {
        throw std::invalid_argument("Process name is null");
    }

    auto it = mock_processes_.find(name);
    if (it == mock_processes_.end()) {
        throw std::runtime_error("Process not found: " + std::string(name));
    }

    return it->second;
}

std::vector<DWORD> MockDMA::get_process_id_list(const char* name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!name) {
        throw std::invalid_argument("Process name is null");
    }

    std::vector<DWORD> result;
    auto it = mock_processes_.find(name);
    if (it != mock_processes_.end()) {
        result.push_back(it->second);
    }

    return result;
}

} // namespace ArgoSentry
