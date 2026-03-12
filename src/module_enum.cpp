// ArgoSentry Module Enumerator Implementation
// Module enumeration and analysis

#include "ArgoSentry/module_enum.hh"
#include "ArgoSentry/dma.hh"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iomanip>

// VMM API for module enumeration
extern "C" {
    #include "external/vmm/vmmdll.h"
}

namespace ArgoSentry {

// ============================================================================
// ModuleInfo Implementation
// ============================================================================

bool ModuleInfo::is_main_module() const noexcept {
    // Main module typically ends with .exe
    if (name.length() < 4) return false;
    
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    return lower_name.ends_with(".exe");
}

// ============================================================================
// ModuleEnumerator Implementation
// ============================================================================

ModuleEnumerator::ModuleEnumerator(DMA* dma, DWORD process_id)
    : dma_(dma)
    , process_id_(process_id)
    , cache_enabled_(true)
{
}

std::vector<ModuleInfo> ModuleEnumerator::enumerate_modules(bool force_refresh) {
    // Return cached modules if available and not forcing refresh
    if (!force_refresh && cache_enabled_ && !modules_.empty()) {
        return modules_;
    }
    
    // Refresh module list
    if (!refresh_modules()) {
        return {}; // Return empty on error
    }
    
    return modules_;
}

std::optional<ModuleInfo> ModuleEnumerator::find_module(const std::string& module_name) {
    // Ensure modules are loaded
    if (modules_.empty()) {
        enumerate_modules();
    }
    
    // Normalize search name
    std::string search_name = normalize_module_name(module_name);
    
    // Search for module (case-insensitive)
    for (const auto& mod : modules_) {
        if (normalize_module_name(mod.name) == search_name) {
            return mod;
        }
    }
    
    return std::nullopt;
}

std::optional<ModuleInfo> ModuleEnumerator::find_module_by_address(uint64_t address) {
    // Ensure modules are loaded
    if (modules_.empty()) {
        enumerate_modules();
    }
    
    // Find module containing address
    for (const auto& mod : modules_) {
        if (mod.contains_address(address)) {
            return mod;
        }
    }
    
    return std::nullopt;
}

std::optional<ModuleInfo> ModuleEnumerator::get_main_module() {
    // Ensure modules are loaded
    if (modules_.empty()) {
        enumerate_modules();
    }
    
    // Find main executable
    for (const auto& mod : modules_) {
        if (mod.is_main_module()) {
            return mod;
        }
    }
    
    return std::nullopt;
}

std::optional<uint64_t> ModuleEnumerator::get_module_base(const std::string& module_name) {
    auto mod = find_module(module_name);
    if (mod) {
        return mod->base_address;
    }
    return std::nullopt;
}

std::optional<uint32_t> ModuleEnumerator::get_module_size(const std::string& module_name) {
    auto mod = find_module(module_name);
    if (mod) {
        return mod->size;
    }
    return std::nullopt;
}

std::optional<uint64_t> ModuleEnumerator::calculate_rva(
    uint64_t absolute_address,
    const std::optional<std::string>& module_name
) {
    std::optional<ModuleInfo> mod;
    
    if (module_name.has_value()) {
        // Find specific module
        mod = find_module(*module_name);
    } else {
        // Auto-detect module by address
        mod = find_module_by_address(absolute_address);
    }
    
    if (mod && mod->contains_address(absolute_address)) {
        return absolute_address - mod->base_address;
    }
    
    return std::nullopt;
}

std::optional<uint64_t> ModuleEnumerator::calculate_absolute(
    uint64_t rva,
    const std::string& module_name
) {
    auto mod = find_module(module_name);
    if (mod) {
        return mod->base_address + rva;
    }
    return std::nullopt;
}

void ModuleEnumerator::clear_cache() noexcept {
    modules_.clear();
}

bool ModuleEnumerator::export_to_file(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header
    file << "Module Enumeration for PID: " << process_id_ << "\n";
    file << "Total Modules: " << modules_.size() << "\n";
    file << std::string(80, '=') << "\n\n";
    
    // Write module list
    for (const auto& mod : modules_) {
        file << "Module: " << mod.name << "\n";
        file << "  Path: " << mod.full_path << "\n";
        file << "  Base: 0x" << std::hex << std::uppercase << std::setw(16) 
             << std::setfill('0') << mod.base_address << "\n";
        file << "  Size: 0x" << std::hex << std::uppercase << std::setw(8) 
             << mod.size << " (" << std::dec << mod.size << " bytes)\n";
        file << "  End:  0x" << std::hex << std::uppercase << std::setw(16) 
             << mod.end_address() << "\n";
        file << "  Entry: 0x" << std::hex << std::uppercase << std::setw(16) 
             << mod.entry_point << "\n";
        file << "\n";
    }
    
    return true;
}

std::string ModuleEnumerator::normalize_module_name(const std::string& name) const {
    std::string normalized = name;
    
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    // Extract filename from path if present
    size_t last_slash = normalized.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        normalized = normalized.substr(last_slash + 1);
    }
    
    return normalized;
}

bool ModuleEnumerator::refresh_modules() {
    if (!dma_) {
        return false;
    }
    
    modules_.clear();
    
    // Use VMM API to enumerate modules
    PVMMDLL_MAP_MODULE pModuleMap = nullptr;
    PVMMDLL_MAP_MODULEENTRY pModuleEntry = nullptr;
    
    // Get VMM handle from DMA
    auto* handle = dma_->get_handle();
    if (!handle) {
        return false;
    }
    
    // Retrieve module map
    if (!VMMDLL_Map_GetModuleU(handle, process_id_, &pModuleMap, 0)) {
        return false;
    }
    
    // Process each module
    for (DWORD i = 0; i < pModuleMap->cMap; i++) {
        pModuleEntry = &pModuleMap->pMap[i];
        
        ModuleInfo info;
        info.name = pModuleEntry->uszText;
        info.full_path = pModuleEntry->uszFullName;
        info.base_address = pModuleEntry->vaBase;
        info.size = pModuleEntry->cbImageSize;
        info.entry_point = pModuleEntry->vaEntry;
        
        modules_.push_back(info);
    }
    
    // Free module map
    VMMDLL_MemFree(pModuleMap);
    
    // Sort by base address
    std::sort(modules_.begin(), modules_.end(),
              [](const ModuleInfo& a, const ModuleInfo& b) {
                  return a.base_address < b.base_address;
              });
    
    return true;
}

// ============================================================================
// ModuleManager Implementation
// ============================================================================

void ModuleManager::add_process(DWORD process_id, ModuleEnumerator&& enumerator) {
    enumerators_[process_id] = std::move(enumerator);
}

ModuleEnumerator* ModuleManager::get_enumerator(DWORD process_id) noexcept {
    auto it = enumerators_.find(process_id);
    if (it != enumerators_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ModuleManager::remove_process(DWORD process_id) {
    return enumerators_.erase(process_id) > 0;
}

void ModuleManager::clear() noexcept {
    enumerators_.clear();
}

} // namespace ArgoSentry
