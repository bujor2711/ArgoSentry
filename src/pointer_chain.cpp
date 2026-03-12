// pointer_chain.cpp - Pointer Chain Resolver Implementation
// Enables automatic resolution of multi-level pointer chains for reverse engineering

#include "../include/ArgoSentry/pointer_chain.hh"
#include "../include/ArgoSentry/dma.hh"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace ArgoSentry {

// ============================================================================
// PointerChain Implementation
// ============================================================================

PointerChain::PointerChain(uint64_t base_address, const std::vector<uint64_t>& offsets)
    : base_address_(base_address)
    , offsets_(offsets)
    , cache_enabled_(false)
    , cache_ttl_ms_(1000)
    , cached_address_(std::nullopt)
{
}

PointerChain::PointerChain(const std::vector<uint64_t>& chain_addresses)
    : base_address_(chain_addresses.empty() ? 0 : chain_addresses[0])
    , cache_enabled_(false)
    , cache_ttl_ms_(1000)
    , cached_address_(std::nullopt)
{
    if (chain_addresses.size() > 1) {
        offsets_.assign(chain_addresses.begin() + 1, chain_addresses.end());
    }
}

std::optional<uint64_t> PointerChain::resolve(DMA& dma, DWORD process_id)
{
    // Check cache first
    if (cache_enabled_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        if (is_cache_valid()) {
            return cached_address_;
        }
    }

    // Start with base address
    uint64_t current_address = base_address_;

    // Follow pointer chain
    for (size_t i = 0; i < offsets_.size(); ++i) {
        // Read pointer at current address
        auto ptr_result = dma.read<uint64_t>(current_address, process_id);
        if (!ptr_result.has_value()) {
            return std::nullopt; // Chain broken
        }

        uint64_t ptr_value = ptr_result.value();
        
        // Add offset to get next address
        current_address = ptr_value + offsets_[i];

        // Validate address is not null
        if (current_address == 0 || current_address == offsets_[i]) {
            return std::nullopt; // Invalid pointer
        }
    }

    // Update cache
    if (cache_enabled_) {
        update_cache(current_address);
    }

    return current_address;
}

std::optional<uint64_t> PointerChain::resolve(
    std::function<std::optional<uint64_t>(uint64_t)> read_func)
{
    // Check cache first
    if (cache_enabled_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        if (is_cache_valid()) {
            return cached_address_;
        }
    }

    // Start with base address
    uint64_t current_address = base_address_;

    // Follow pointer chain
    for (size_t i = 0; i < offsets_.size(); ++i) {
        // Read pointer at current address using custom function
        auto ptr_result = read_func(current_address);
        if (!ptr_result.has_value()) {
            return std::nullopt; // Chain broken
        }

        uint64_t ptr_value = ptr_result.value();
        
        // Add offset to get next address
        current_address = ptr_value + offsets_[i];

        // Validate address is not null
        if (current_address == 0 || current_address == offsets_[i]) {
            return std::nullopt; // Invalid pointer
        }
    }

    // Update cache
    if (cache_enabled_) {
        update_cache(current_address);
    }

    return current_address;
}

bool PointerChain::validate(DMA& dma, DWORD process_id)
{
    uint64_t current_address = base_address_;

    // Check base address is valid
    if (current_address == 0) {
        return false;
    }

    // Validate each level of the chain
    for (size_t i = 0; i < offsets_.size(); ++i) {
        // Try to read pointer at current address
        auto ptr_result = dma.read<uint64_t>(current_address, process_id);
        if (!ptr_result.has_value()) {
            return false; // Cannot read at this address
        }

        uint64_t ptr_value = ptr_result.value();
        
        // Calculate next address
        current_address = ptr_value + offsets_[i];

        // Validate address is not null
        if (current_address == 0 || current_address == offsets_[i]) {
            return false; // Invalid pointer
        }
    }

    return true; // All levels validated successfully
}

const std::vector<uint64_t>& PointerChain::offsets() const noexcept
{
    return offsets_;
}

void PointerChain::add_offset(uint64_t offset)
{
    offsets_.push_back(offset);
    invalidate_cache();
}

void PointerChain::set_base_address(uint64_t base) noexcept
{
    base_address_ = base;
    invalidate_cache();
}

void PointerChain::clear_offsets() noexcept
{
    offsets_.clear();
    invalidate_cache();
}

void PointerChain::enable_cache(bool enable, uint32_t ttl_ms)
{
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_enabled_ = enable;
    cache_ttl_ms_ = ttl_ms;
    if (!enable) {
        cached_address_ = std::nullopt;
    }
}

void PointerChain::invalidate_cache() noexcept
{
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_address_ = std::nullopt;
}

std::string PointerChain::to_string() const
{
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    
    // Format: [0x140000000] -> [+0x10] -> [+0x20] -> [+0x08]
    oss << "[0x" << std::setfill('0') << std::setw(8) << base_address_ << "]";
    
    for (const auto& offset : offsets_) {
        oss << " -> [+0x" << std::setfill('0') << std::setw(2) << offset << "]";
    }
    
    return oss.str();
}

std::optional<PointerChain> PointerChain::from_string(const std::string& chain_str)
{
    if (chain_str.empty()) {
        return std::nullopt;
    }

    std::vector<uint64_t> addresses;
    std::istringstream iss(chain_str);
    std::string token;

    // Parse format: "0x140000000+0x10+0x20+0x08" or "140000000+10+20+08"
    while (std::getline(iss, token, '+')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);

        if (token.empty()) {
            continue;
        }

        // Remove "0x" or "0X" prefix if present
        if (token.size() >= 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
            token = token.substr(2);
        }

        // Convert hex string to uint64_t
        try {
            uint64_t value = std::stoull(token, nullptr, 16);
            addresses.push_back(value);
        }
        catch (...) {
            return std::nullopt; // Parsing failed
        }
    }

    if (addresses.empty()) {
        return std::nullopt;
    }

    // First address is base, rest are offsets
    uint64_t base = addresses[0];
    std::vector<uint64_t> offsets(addresses.begin() + 1, addresses.end());

    return PointerChain(base, offsets);
}

bool PointerChain::is_cache_valid() const
{
    if (!cached_address_.has_value()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - cache_timestamp_).count();

    return elapsed < cache_ttl_ms_;
}

void PointerChain::update_cache(uint64_t address)
{
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_address_ = address;
    cache_timestamp_ = std::chrono::steady_clock::now();
}

// ============================================================================
// PointerChainManager Implementation
// ============================================================================

void PointerChainManager::add_chain(const std::string& name, const PointerChain& chain)
{
    std::lock_guard<std::mutex> lock(mutex_);
    chains_[name] = chain;
}

std::optional<PointerChain> PointerChainManager::get_chain(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = chains_.find(name);
    if (it != chains_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool PointerChainManager::remove_chain(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return chains_.erase(name) > 0;
}

std::optional<uint64_t> PointerChainManager::resolve(
    const std::string& name, DMA& dma, DWORD process_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = chains_.find(name);
    if (it == chains_.end()) {
        return std::nullopt;
    }

    // Unlock before resolve (resolve may take time)
    PointerChain chain_copy = it->second;
    mutex_.unlock();

    return chain_copy.resolve(dma, process_id);
}

std::map<std::string, std::optional<uint64_t>> PointerChainManager::resolve_all(
    DMA& dma, DWORD process_id)
{
    std::map<std::string, std::optional<uint64_t>> results;

    // Copy chains to avoid holding lock during resolution
    std::map<std::string, PointerChain> chains_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        chains_copy = chains_;
    }

    // Resolve each chain
    for (const auto& [name, chain] : chains_copy) {
        results[name] = chain.resolve(dma, process_id);
    }

    return results;
}

std::map<std::string, bool> PointerChainManager::validate_all(DMA& dma, DWORD process_id)
{
    std::map<std::string, bool> results;

    // Copy chains to avoid holding lock during validation
    std::map<std::string, PointerChain> chains_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        chains_copy = chains_;
    }

    // Validate each chain
    for (const auto& [name, chain] : chains_copy) {
        results[name] = chain.validate(dma, process_id);
    }

    return results;
}

std::vector<std::string> PointerChainManager::get_chain_names() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(chains_.size());
    
    for (const auto& [name, _] : chains_) {
        names.push_back(name);
    }
    
    return names;
}

void PointerChainManager::clear() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    chains_.clear();
}

size_t PointerChainManager::size() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return chains_.size();
}

bool PointerChainManager::save_to_file(const std::string& filepath) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    // Simple JSON format (manual serialization)
    file << "{\n";
    file << "  \"chains\": [\n";

    bool first = true;
    for (const auto& [name, chain] : chains_) {
        if (!first) {
            file << ",\n";
        }
        first = false;

        file << "    {\n";
        file << "      \"name\": \"" << name << "\",\n";
        file << "      \"base\": \"0x" << std::hex << std::uppercase 
             << std::setfill('0') << std::setw(8) << chain.base_address() << "\",\n";
        file << "      \"offsets\": [";

        const auto& offsets = chain.offsets();
        for (size_t i = 0; i < offsets.size(); ++i) {
            if (i > 0) file << ", ";
            file << "\"0x" << std::hex << std::uppercase 
                 << std::setfill('0') << std::setw(2) << offsets[i] << "\"";
        }

        file << "]\n";
        file << "    }";
    }

    file << "\n  ]\n";
    file << "}\n";

    return true;
}

bool PointerChainManager::load_from_file(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    // Simple JSON parsing (manual parsing)
    std::string line;
    std::string current_name;
    uint64_t current_base = 0;
    std::vector<uint64_t> current_offsets;
    bool in_offsets = false;

    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n,") + 1);

        if (line.find("\"name\":") != std::string::npos) {
            // Parse name: "name": "player_health"
            auto start = line.find("\"", line.find(":") + 1) + 1;
            auto end = line.find("\"", start);
            current_name = line.substr(start, end - start);
        }
        else if (line.find("\"base\":") != std::string::npos) {
            // Parse base: "base": "0x140000000"
            auto start = line.find("\"", line.find(":") + 1) + 1;
            auto end = line.find("\"", start);
            std::string hex_str = line.substr(start, end - start);
            
            // Remove 0x prefix
            if (hex_str.size() >= 2 && hex_str[0] == '0' && hex_str[1] == 'x') {
                hex_str = hex_str.substr(2);
            }
            
            current_base = std::stoull(hex_str, nullptr, 16);
        }
        else if (line.find("\"offsets\":") != std::string::npos) {
            in_offsets = true;
            current_offsets.clear();

            // Parse offsets array: "offsets": ["0x10", "0x20"]
            auto start = line.find("[");
            auto end = line.find("]");
            if (start != std::string::npos && end != std::string::npos) {
                std::string offsets_str = line.substr(start + 1, end - start - 1);
                std::istringstream iss(offsets_str);
                std::string offset_token;

                while (std::getline(iss, offset_token, ',')) {
                    // Trim and remove quotes
                    offset_token.erase(0, offset_token.find_first_not_of(" \t\r\n\""));
                    offset_token.erase(offset_token.find_last_not_of(" \t\r\n\"") + 1);

                    if (offset_token.empty()) continue;

                    // Remove 0x prefix
                    if (offset_token.size() >= 2 && offset_token[0] == '0' && 
                        (offset_token[1] == 'x' || offset_token[1] == 'X')) {
                        offset_token = offset_token.substr(2);
                    }

                    current_offsets.push_back(std::stoull(offset_token, nullptr, 16));
                }
            }
        }
        else if (line.find("}") != std::string::npos && !current_name.empty()) {
            // End of chain object, add to manager
            PointerChain chain(current_base, current_offsets);
            add_chain(current_name, chain);

            // Reset for next chain
            current_name.clear();
            current_base = 0;
            current_offsets.clear();
            in_offsets = false;
        }
    }

    return true;
}

} // namespace ArgoSentry
