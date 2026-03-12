// ArgoSentry Memory Structure Templates v3.1
// Implementation

#include "ArgoSentry/memory_struct.hh"
#include <algorithm>

namespace ArgoSentry {

// MemoryStructManager implementation

void MemoryStructManager::register_struct(
    const std::string& name,
    uint64_t base_address,
    uint64_t offset
) {
    structs_[name] = StructInfo{base_address, offset};
}

std::optional<uint64_t> MemoryStructManager::get_address(const std::string& name) const {
    auto it = structs_.find(name);
    if (it == structs_.end()) {
        return std::nullopt;
    }
    return it->second.base_address + it->second.offset;
}

void MemoryStructManager::update_base(uint64_t old_base, uint64_t new_base) {
    for (auto& [name, info] : structs_) {
        if (info.base_address == old_base) {
            info.base_address = new_base;
        }
    }
}

bool MemoryStructManager::unregister_struct(const std::string& name) {
    return structs_.erase(name) > 0;
}

void MemoryStructManager::clear() noexcept {
    structs_.clear();
}

std::vector<std::string> MemoryStructManager::get_names() const {
    std::vector<std::string> names;
    names.reserve(structs_.size());
    
    for (const auto& [name, _] : structs_) {
        names.push_back(name);
    }
    
    return names;
}

} // namespace ArgoSentry
