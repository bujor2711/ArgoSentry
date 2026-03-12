// DMA Batch Operations Integration
// This file contains the batch operations integration for DMA class

#include "ArgoSentry/dma.hh"
#include "ArgoSentry/batch.hh"
#include "ArgoSentry/logger.hh"  // v2.9 - Logging framework
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace ArgoSentry {

// Batch read - delegates to BatchOperations
BatchReadResult DMA::batch_read(
    std::vector<ReadRequest>& requests,
    DWORD process_id,
    bool optimize
) {
    if (!batch_ops_) {
        if (logger_) {
            LOG_ERROR(logger_, "Batch operations not initialized");
        }
        throw std::runtime_error("Batch operations not initialized");
    }

    // ✅ Log batch read start (v2.9)
    if (logger_) {
        std::stringstream ss;
        size_t total_bytes = 0;
        for (const auto& req : requests) {
            total_bytes += req.size;
        }
        ss << "Batch read: " << requests.size() << " requests, " 
           << (total_bytes / 1024) << " KB total (PID " << process_id 
           << ", optimize=" << (optimize ? "true" : "false") << ")";
        LOG_INFO(logger_, ss.str());
    }

    auto result = batch_ops_->batch_read(requests, process_id, optimize);

    // ✅ Log batch read result (v2.9)
    if (logger_) {
        std::stringstream ss;
        ss << "Batch complete: " << result.successful_reads << "/" << requests.size() 
           << " successful, " << std::fixed << std::setprecision(2) 
           << result.throughput_mbps << " MB/s";
        LOG_INFO(logger_, ss.str());

        // ✅ Warning for failed reads (v2.9)
        if (result.failed_reads > 0) {
            std::stringstream warn_ss;
            warn_ss << "Batch had " << result.failed_reads << " failures ("
                   << ((result.failed_reads * 100) / requests.size()) << "%)";
            LOG_WARN(logger_, warn_ss.str());
        }

        // ✅ Performance warning for low throughput <50 MB/s (v2.9)
        if (result.throughput_mbps < 50.0) {
            std::stringstream warn_ss;
            warn_ss << "Low batch throughput: " << std::fixed << std::setprecision(2)
                   << result.throughput_mbps << " MB/s (expected >50 MB/s)";
            LOG_WARN(logger_, warn_ss.str());
        }
    }

    return result;
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


