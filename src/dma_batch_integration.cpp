// DMA Batch Operations Integration
// This file contains the batch operations integration for DMA class

#include "ArgoSentry/dma.hh"
#include "ArgoSentry/batch.hh"
#include <stdexcept>

namespace ArgoSentry {

// Batch read - delegates to BatchOperations
BatchReadResult DMA::batch_read(
    std::vector<ReadRequest>& requests,
    DWORD process_id,
    bool optimize
) {
    if (!batch_ops_) {
        throw std::runtime_error("Batch operations not initialized");
    }
    
    return batch_ops_->batch_read(requests, process_id, optimize);
}

// Type-safe batch read - delegates to BatchOperations
template<typename T>
std::vector<std::optional<T>> DMA::batch_read_typed(
    const std::vector<uint64_t>& addresses,
    DWORD process_id
) {
    if (!batch_ops_) {
        throw std::runtime_error("Batch operations not initialized");
    }
    
    return batch_ops_->batch_read_typed<T>(addresses, process_id);
}

// Range batch read - delegates to BatchOperations
std::vector<uint8_t> DMA::batch_read_range(
    uint64_t start_address,
    uint64_t end_address,
    size_t chunk_size,
    DWORD process_id
) {
    if (!batch_ops_) {
        throw std::runtime_error("Batch operations not initialized");
    }
    
    return batch_ops_->batch_read_range(start_address, end_address, chunk_size, process_id);
}

// Get batch operations reference
const BatchOperations& DMA::get_batch_operations() const {
    if (!batch_ops_) {
        throw std::runtime_error("Batch operations not initialized");
    }
    
    return *batch_ops_;
}

// Explicit template instantiations for common types
template std::vector<std::optional<uint8_t>> DMA::batch_read_typed<uint8_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<uint16_t>> DMA::batch_read_typed<uint16_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<uint32_t>> DMA::batch_read_typed<uint32_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<uint64_t>> DMA::batch_read_typed<uint64_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<int8_t>> DMA::batch_read_typed<int8_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<int16_t>> DMA::batch_read_typed<int16_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<int32_t>> DMA::batch_read_typed<int32_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<int64_t>> DMA::batch_read_typed<int64_t>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<float>> DMA::batch_read_typed<float>(const std::vector<uint64_t>&, DWORD);
template std::vector<std::optional<double>> DMA::batch_read_typed<double>(const std::vector<uint64_t>&, DWORD);

} // namespace ArgoSentry


