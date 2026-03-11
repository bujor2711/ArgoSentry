#include "../include/ArgoSentry/dma.hh"
#include "../include/ArgoSentry/differ.hh"
#include <stdexcept>

namespace ArgoSentry {

// Wrapper methods for Memory Diffing

std::vector<MemoryDiff> DMA::compare_memory_snapshots(
    const std::string& snapshot1,
    const std::string& snapshot2)
{
    if (!memory_differ_) {
        throw std::runtime_error("MemoryDiffer not initialized");
    }
    return memory_differ_->compare_snapshots(snapshot1, snapshot2);
}

std::vector<uint64_t> DMA::find_changed_addresses(
    uint64_t start_address,
    uint64_t end_address,
    DWORD process_id,
    std::chrono::milliseconds interval)
{
    if (!memory_differ_) {
        throw std::runtime_error("MemoryDiffer not initialized");
    }
    return memory_differ_->find_changed_addresses(*this, start_address, end_address, process_id, interval);
}

std::vector<MemoryDiff> DMA::compare_memory_regions(
    uint64_t start_address,
    uint64_t end_address,
    DWORD process_id,
    std::chrono::milliseconds interval)
{
    if (!memory_differ_) {
        throw std::runtime_error("MemoryDiffer not initialized");
    }
    return memory_differ_->compare_regions(*this, start_address, end_address, process_id, interval);
}

std::vector<uint64_t> DMA::find_memory_value(
    uint64_t start_address,
    uint64_t end_address,
    DWORD process_id,
    const std::vector<uint8_t>& value)
{
    if (!memory_differ_) {
        throw std::runtime_error("MemoryDiffer not initialized");
    }
    return memory_differ_->find_value(*this, start_address, end_address, process_id, value);
}

std::vector<uint64_t> DMA::filter_changed_addresses(
    const std::vector<uint64_t>& addresses,
    DWORD process_id,
    std::chrono::milliseconds interval)
{
    if (!memory_differ_) {
        throw std::runtime_error("MemoryDiffer not initialized");
    }
    return memory_differ_->filter_changed(*this, addresses, process_id, interval);
}

const MemoryDiffer& DMA::get_memory_differ() const {
    if (!memory_differ_) {
        throw std::runtime_error("MemoryDiffer not initialized");
    }
    return *memory_differ_;
}

} // namespace ArgoSentry
