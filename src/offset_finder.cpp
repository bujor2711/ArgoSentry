// ArgoSentry Offset Finder Implementation
// AOB to RVA conversion for portable addressing

#include "ArgoSentry/offset_finder.hh"
#include "ArgoSentry/dma.hh"
#include "ArgoSentry/module_enum.hh"
#include "ArgoSentry/pattern_scanner_enhanced.hh"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ArgoSentry {

// ============================================================================
// OffsetFinder Implementation
// ============================================================================

OffsetFinder::OffsetFinder(
    DMA* dma,
    EnhancedPatternScanner* scanner,
    ModuleEnumerator* enumerator,
    DWORD process_id
)
    : dma_(dma)
    , scanner_(scanner)
    , enumerator_(enumerator)
    , process_id_(process_id)
{
}

std::optional<OffsetInfo> OffsetFinder::find_offset(
    const std::string& name,
    const std::string& module_name,
    const std::string& signature,
    std::optional<uint64_t> search_start,
    std::optional<uint32_t> search_size
) {
    // Find signature in module
    auto absolute_addr = find_signature_in_module(
        module_name,
        signature,
        search_start,
        search_size
    );
    
    if (!absolute_addr) {
        return std::nullopt;
    }
    
    // Calculate RVA
    auto rva = enumerator_->calculate_rva(*absolute_addr, module_name);
    if (!rva) {
        return std::nullopt;
    }
    
    // Create offset info
    OffsetInfo info;
    info.name = name;
    info.module_name = module_name;
    info.rva = *rva;
    info.signature = signature;
    info.last_absolute_address = *absolute_addr;
    info.is_valid = true;
    
    // Register offset
    offsets_[name] = info;
    
    return info;
}

bool OffsetFinder::register_offset(
    const std::string& name,
    const std::string& module_name,
    uint64_t rva,
    const std::string& signature
) {
    // Get module base to calculate absolute address
    auto module_base = enumerator_->get_module_base(module_name);
    if (!module_base) {
        return false;
    }
    
    OffsetInfo info;
    info.name = name;
    info.module_name = module_name;
    info.rva = rva;
    info.signature = signature;
    info.last_absolute_address = *module_base + rva;
    info.is_valid = true;
    
    offsets_[name] = info;
    return true;
}

std::optional<OffsetInfo> OffsetFinder::get_offset(const std::string& name) const {
    auto it = offsets_.find(name);
    if (it != offsets_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<uint64_t> OffsetFinder::get_absolute_address(const std::string& name) {
    auto offset = get_offset(name);
    if (!offset) {
        return std::nullopt;
    }
    
    // Get current module base
    auto module_base = enumerator_->get_module_base(offset->module_name);
    if (!module_base) {
        return std::nullopt;
    }
    
    // Calculate absolute address
    uint64_t absolute = offset->get_absolute(*module_base);
    
    // Update cached absolute address
    offsets_[name].last_absolute_address = absolute;
    
    return absolute;
}

size_t OffsetFinder::update_all_offsets() {
    size_t updated_count = 0;
    
    for (auto& [name, offset] : offsets_) {
        // Skip offsets without signatures
        if (offset.signature.empty()) {
            continue;
        }
        
        // Re-scan for signature
        auto absolute_addr = find_signature_in_module(
            offset.module_name,
            offset.signature,
            std::nullopt,
            std::nullopt
        );
        
        if (absolute_addr) {
            // Recalculate RVA
            auto rva = enumerator_->calculate_rva(*absolute_addr, offset.module_name);
            if (rva) {
                offset.rva = *rva;
                offset.last_absolute_address = *absolute_addr;
                offset.is_valid = true;
                updated_count++;
            }
        } else {
            offset.is_valid = false;
        }
    }
    
    return updated_count;
}

bool OffsetFinder::validate_offset(const std::string& name) {
    auto offset = get_offset(name);
    if (!offset) {
        return false;
    }
    
    // Get current absolute address
    auto addr = get_absolute_address(name);
    if (!addr) {
        offsets_[name].is_valid = false;
        return false;
    }
    
    // Try to read at address (basic validation)
    auto test_read = dma_->read<uint64_t>(*addr, process_id_);
    bool is_valid = test_read.has_value();
    
    offsets_[name].is_valid = is_valid;
    return is_valid;
}

std::map<std::string, bool> OffsetFinder::validate_all_offsets() {
    std::map<std::string, bool> results;
    
    for (const auto& [name, offset] : offsets_) {
        results[name] = validate_offset(name);
    }
    
    return results;
}

bool OffsetFinder::remove_offset(const std::string& name) {
    return offsets_.erase(name) > 0;
}

void OffsetFinder::clear() noexcept {
    offsets_.clear();
}

std::vector<std::string> OffsetFinder::get_offset_names() const {
    std::vector<std::string> names;
    names.reserve(offsets_.size());
    
    for (const auto& [name, _] : offsets_) {
        names.push_back(name);
    }
    
    return names;
}

bool OffsetFinder::export_to_file(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // JSON format
    file << "{\n";
    file << "  \"offsets\": [\n";
    
    bool first = true;
    for (const auto& [name, offset] : offsets_) {
        if (!first) {
            file << ",\n";
        }
        first = false;
        
        file << "    {\n";
        file << "      \"name\": \"" << name << "\",\n";
        file << "      \"module\": \"" << offset.module_name << "\",\n";
        file << "      \"rva\": \"0x" << std::hex << std::uppercase 
             << std::setw(8) << std::setfill('0') << offset.rva << "\",\n";
        file << "      \"signature\": \"" << offset.signature << "\",\n";
        file << "      \"valid\": " << (offset.is_valid ? "true" : "false") << "\n";
        file << "    }";
    }
    
    file << "\n  ]\n";
    file << "}\n";
    
    return true;
}

size_t OffsetFinder::import_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return 0;
    }
    
    size_t imported = 0;
    std::string line;
    std::string current_name;
    std::string current_module;
    uint64_t current_rva = 0;
    std::string current_signature;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n,") + 1);
        
        if (line.find("\"name\":") != std::string::npos) {
            auto start = line.find("\"", line.find(":") + 1) + 1;
            auto end = line.find("\"", start);
            current_name = line.substr(start, end - start);
        }
        else if (line.find("\"module\":") != std::string::npos) {
            auto start = line.find("\"", line.find(":") + 1) + 1;
            auto end = line.find("\"", start);
            current_module = line.substr(start, end - start);
        }
        else if (line.find("\"rva\":") != std::string::npos) {
            auto start = line.find("\"", line.find(":") + 1) + 1;
            auto end = line.find("\"", start);
            std::string hex_str = line.substr(start, end - start);
            
            // Remove 0x prefix
            if (hex_str.size() >= 2 && hex_str[0] == '0' && hex_str[1] == 'x') {
                hex_str = hex_str.substr(2);
            }
            
            current_rva = std::stoull(hex_str, nullptr, 16);
        }
        else if (line.find("\"signature\":") != std::string::npos) {
            auto start = line.find("\"", line.find(":") + 1) + 1;
            auto end = line.find("\"", start);
            current_signature = line.substr(start, end - start);
        }
        else if (line.find("}") != std::string::npos && !current_name.empty()) {
            // Register offset
            if (register_offset(current_name, current_module, current_rva, current_signature)) {
                imported++;
            }
            
            // Reset for next offset
            current_name.clear();
            current_module.clear();
            current_rva = 0;
            current_signature.clear();
        }
    }
    
    return imported;
}

std::optional<uint64_t> OffsetFinder::signature_to_offset(
    const std::string& module_name,
    const std::string& signature
) {
    auto absolute_addr = find_signature_in_module(
        module_name,
        signature,
        std::nullopt,
        std::nullopt
    );
    
    if (!absolute_addr) {
        return std::nullopt;
    }
    
    return enumerator_->calculate_rva(*absolute_addr, module_name);
}

std::optional<uint64_t> OffsetFinder::find_signature_in_module(
    const std::string& module_name,
    const std::string& signature,
    std::optional<uint64_t> search_start,
    std::optional<uint32_t> search_size
) {
    if (!scanner_) {
        return std::nullopt;
    }
    
    // Get module info
    auto module = enumerator_->find_module(module_name);
    if (!module) {
        return std::nullopt;
    }
    
    // Determine search range
    uint64_t start = search_start.value_or(module->base_address);
    uint32_t size = search_size.value_or(module->size);
    uint64_t end = start + size;
    
    // Scan for pattern
    auto results = scanner_->scan_pattern(
        signature,
        start,
        end,
        true  // First match only
    );
    
    if (results.empty()) {
        return std::nullopt;
    }
    
    return results[0];
}

// ============================================================================
// OffsetDatabase Implementation
// ============================================================================

void OffsetDatabase::add_game(const std::string& game_name, const std::vector<OffsetInfo>& offsets) {
    database_[game_name] = offsets;
}

std::optional<std::vector<OffsetInfo>> OffsetDatabase::get_game_offsets(
    const std::string& game_name
) const {
    auto it = database_.find(game_name);
    if (it != database_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool OffsetDatabase::remove_game(const std::string& game_name) {
    return database_.erase(game_name) > 0;
}

void OffsetDatabase::clear() noexcept {
    database_.clear();
}

std::vector<std::string> OffsetDatabase::get_game_names() const {
    std::vector<std::string> names;
    names.reserve(database_.size());
    
    for (const auto& [name, _] : database_) {
        names.push_back(name);
    }
    
    return names;
}

bool OffsetDatabase::export_to_file(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"games\": [\n";
    
    bool first_game = true;
    for (const auto& [game_name, offsets] : database_) {
        if (!first_game) {
            file << ",\n";
        }
        first_game = false;
        
        file << "    {\n";
        file << "      \"name\": \"" << game_name << "\",\n";
        file << "      \"offsets\": [\n";
        
        bool first_offset = true;
        for (const auto& offset : offsets) {
            if (!first_offset) {
                file << ",\n";
            }
            first_offset = false;
            
            file << "        {\n";
            file << "          \"name\": \"" << offset.name << "\",\n";
            file << "          \"module\": \"" << offset.module_name << "\",\n";
            file << "          \"rva\": \"0x" << std::hex << offset.rva << "\"\n";
            file << "        }";
        }
        
        file << "\n      ]\n";
        file << "    }";
    }
    
    file << "\n  ]\n";
    file << "}\n";
    
    return true;
}

size_t OffsetDatabase::import_from_file(const std::string& filepath) {
    // Simplified import - would need proper JSON parsing for production
    return 0;
}

} // namespace ArgoSentry
