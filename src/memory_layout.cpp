#include "include/ArgoSentry/memory_layout.hh"
#include "external/vmm/vmmdll.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cctype>

// Undefine Windows.h macros that conflict with our enum class
#ifdef PAGE_NOACCESS
#undef PAGE_NOACCESS
#endif
#ifdef PAGE_READONLY
#undef PAGE_READONLY
#endif
#ifdef PAGE_READWRITE
#undef PAGE_READWRITE
#endif
#ifdef PAGE_WRITECOPY
#undef PAGE_WRITECOPY
#endif
#ifdef PAGE_EXECUTE
#undef PAGE_EXECUTE
#endif
#ifdef PAGE_EXECUTE_READ
#undef PAGE_EXECUTE_READ
#endif
#ifdef PAGE_EXECUTE_READWRITE
#undef PAGE_EXECUTE_READWRITE
#endif
#ifdef PAGE_EXECUTE_WRITECOPY
#undef PAGE_EXECUTE_WRITECOPY
#endif
#ifdef PAGE_GUARD
#undef PAGE_GUARD
#endif
#ifdef PAGE_NOCACHE
#undef PAGE_NOCACHE
#endif
#ifdef PAGE_WRITECOMBINE
#undef PAGE_WRITECOMBINE
#endif
#ifdef MEM_IMAGE
#undef MEM_IMAGE
#endif
#ifdef MEM_MAPPED
#undef MEM_MAPPED
#endif
#ifdef MEM_PRIVATE
#undef MEM_PRIVATE
#endif
#ifdef MEM_COMMIT
#undef MEM_COMMIT
#endif
#ifdef MEM_FREE
#undef MEM_FREE
#endif
#ifdef MEM_RESERVE
#undef MEM_RESERVE
#endif

namespace ArgoSentry {
namespace MemoryLayout {

// MemoryRegion member functions

bool MemoryRegion::is_executable() const {
    return (protection & static_cast<uint32_t>(Protection::Execute)) ||
           (protection & static_cast<uint32_t>(Protection::ExecuteRead)) ||
           (protection & static_cast<uint32_t>(Protection::ExecuteReadWrite)) ||
           (protection & static_cast<uint32_t>(Protection::ExecuteWriteCopy));
}

bool MemoryRegion::is_writable() const {
    return (protection & static_cast<uint32_t>(Protection::ReadWrite)) ||
           (protection & static_cast<uint32_t>(Protection::WriteCopy)) ||
           (protection & static_cast<uint32_t>(Protection::ExecuteReadWrite)) ||
           (protection & static_cast<uint32_t>(Protection::ExecuteWriteCopy));
}

bool MemoryRegion::is_readable() const {
    return protection != static_cast<uint32_t>(Protection::NoAccess) &&
           !(protection & static_cast<uint32_t>(Protection::Guard));
}

bool MemoryRegion::is_committed() const {
    return state == static_cast<uint32_t>(MemoryState::Commit);
}

bool MemoryRegion::is_image() const {
    return type == static_cast<uint32_t>(MemoryType::Image);
}

uint64_t MemoryRegion::end_address() const {
    return base_address + size;
}

bool MemoryRegion::contains(uint64_t address) const {
    return address >= base_address && address < end_address();
}

// MemoryLayoutAnalyzer implementation

std::vector<MemoryRegion> MemoryLayoutAnalyzer::get_memory_layout(DWORD process_id) const {
    std::vector<MemoryRegion> regions;
    
    // Note: This is a placeholder implementation
    // In real implementation, you would use VMMDLL_Map_GetVadU or similar
    // to enumerate memory regions from the DMA device
    
    // For now, we'll return an empty vector as this requires
    // integration with vmmdll memory mapping APIs
    
    return regions;
}

std::vector<MemoryRegion> MemoryLayoutAnalyzer::get_executable_regions(DWORD process_id) const {
    auto all_regions = get_memory_layout(process_id);
    
    std::vector<MemoryRegion> executable;
    executable.reserve(all_regions.size() / 4); // Estimate
    
    for (const auto& region : all_regions) {
        if (region.is_executable() && region.is_committed()) {
            executable.push_back(region);
        }
    }
    
    return executable;
}

std::vector<MemoryRegion> MemoryLayoutAnalyzer::get_readable_regions(DWORD process_id) const {
    auto all_regions = get_memory_layout(process_id);
    
    std::vector<MemoryRegion> readable;
    readable.reserve(all_regions.size());
    
    for (const auto& region : all_regions) {
        if (region.is_readable() && region.is_committed()) {
            readable.push_back(region);
        }
    }
    
    return readable;
}

std::vector<MemoryRegion> MemoryLayoutAnalyzer::get_module_regions(
    DWORD process_id,
    const std::string& module_name) const {
    
    auto all_regions = get_memory_layout(process_id);
    std::string lowercase_name = to_lowercase(module_name);
    
    std::vector<MemoryRegion> module_regions;
    
    for (const auto& region : all_regions) {
        if (!region.module_name.empty()) {
            std::string region_name = to_lowercase(region.module_name);
            if (region_name.find(lowercase_name) != std::string::npos) {
                module_regions.push_back(region);
            }
        }
    }
    
    return module_regions;
}

std::optional<MemoryRegion> MemoryLayoutAnalyzer::find_module(
    DWORD process_id,
    const std::string& module_name) const {
    
    auto module_regions = get_module_regions(process_id, module_name);
    
    if (module_regions.empty()) {
        return std::nullopt;
    }
    
    // Return the first region (usually the main module header)
    // Sort by base address to ensure we get the lowest address
    auto min_it = std::min_element(module_regions.begin(), module_regions.end(),
        [](const MemoryRegion& a, const MemoryRegion& b) {
            return a.base_address < b.base_address;
        });
    
    if (min_it != module_regions.end()) {
        return *min_it;
    }
    
    return std::nullopt;
}

uint64_t MemoryLayoutAnalyzer::get_total_memory_size(DWORD process_id) const {
    auto all_regions = get_memory_layout(process_id);
    
    uint64_t total_size = 0;
    for (const auto& region : all_regions) {
        if (region.is_committed()) {
            total_size += region.size;
        }
    }
    
    return total_size;
}

std::optional<MemoryRegion> MemoryLayoutAnalyzer::get_region_at_address(
    DWORD process_id,
    uint64_t address) const {
    
    auto all_regions = get_memory_layout(process_id);
    
    for (const auto& region : all_regions) {
        if (region.contains(address)) {
            return region;
        }
    }
    
    return std::nullopt;
}

void MemoryLayoutAnalyzer::print_memory_layout(DWORD process_id) const {
    auto regions = get_memory_layout(process_id);
    
    std::cout << "\n=== Memory Layout for PID " << process_id << " ===\n";
    std::cout << "Total regions: " << regions.size() << "\n\n";
    
    std::cout << std::left;
    std::cout << std::setw(18) << "Base Address" << " | ";
    std::cout << std::setw(12) << "Size" << " | ";
    std::cout << std::setw(6) << "Prot" << " | ";
    std::cout << std::setw(8) << "State" << " | ";
    std::cout << std::setw(8) << "Type" << " | ";
    std::cout << "Module\n";
    std::cout << std::string(90, '-') << "\n";
    
    uint64_t total_size = 0;
    size_t executable_count = 0;
    
    for (const auto& region : regions) {
        // Base address
        std::cout << "0x" << std::hex << std::setw(16) << std::setfill('0') 
                  << region.base_address << std::dec << std::setfill(' ') << " | ";
        
        // Size
        if (region.size >= 1024 * 1024) {
            std::cout << std::setw(10) << (region.size / (1024 * 1024)) << "MB | ";
        } else if (region.size >= 1024) {
            std::cout << std::setw(10) << (region.size / 1024) << "KB | ";
        } else {
            std::cout << std::setw(10) << region.size << " B | ";
        }
        
        // Protection
        std::string prot;
        if (region.is_executable()) prot += "X";
        if (region.is_writable()) prot += "W";
        if (region.is_readable()) prot += "R";
        if (prot.empty()) prot = "-";
        std::cout << std::setw(6) << prot << " | ";
        
        // State
        std::string state_str;
        if (region.state == static_cast<uint32_t>(MemoryState::Commit)) state_str = "COMMIT";
        else if (region.state == static_cast<uint32_t>(MemoryState::Reserve)) state_str = "RESERVE";
        else if (region.state == static_cast<uint32_t>(MemoryState::Free)) state_str = "FREE";
        else state_str = "UNKNOWN";
        std::cout << std::setw(8) << state_str << " | ";

        // Type
        std::string type_str;
        if (region.type == static_cast<uint32_t>(MemoryType::Image)) type_str = "IMAGE";
        else if (region.type == static_cast<uint32_t>(MemoryType::Mapped)) type_str = "MAPPED";
        else if (region.type == static_cast<uint32_t>(MemoryType::Private)) type_str = "PRIVATE";
        else type_str = "UNKNOWN";
        std::cout << std::setw(8) << type_str << " | ";
        
        // Module name
        std::cout << region.module_name << "\n";
        
        // Statistics
        if (region.is_committed()) {
            total_size += region.size;
        }
        if (region.is_executable()) {
            executable_count++;
        }
    }
    
    std::cout << std::string(90, '-') << "\n";
    std::cout << "Total committed memory: " << (total_size / (1024 * 1024)) << " MB\n";
    std::cout << "Executable regions: " << executable_count << "\n";
}

std::string MemoryLayoutAnalyzer::to_lowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace MemoryLayout
} // namespace ArgoSentry


