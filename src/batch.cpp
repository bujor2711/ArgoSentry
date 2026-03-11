#include "VolkDMA/batch.hh"
#include "VolkDMA/validators.hh"
#include <algorithm>
#include <chrono>
#include <map>
#include <cstring>

extern "C" {
    #include "external/vmm/vmmdll.h"
}

namespace VolkDMA {

// Implementation struct
struct BatchOperations::Impl {
    BatchStatistics stats;
    bool stats_enabled;
    void* vmm_handle;  // VMM_HANDLE for DMA operations

    Impl() : stats_enabled(true), vmm_handle(nullptr) {}
};

BatchOperations::BatchOperations()
    : pimpl_(new Impl()) {
}

BatchOperations::~BatchOperations() {
    delete pimpl_;
}

void BatchOperations::set_vmm_handle(void* handle) {
    if (pimpl_) {
        pimpl_->vmm_handle = handle;
    }
}

BatchReadResult BatchOperations::batch_read(
    std::vector<ReadRequest>& requests,
    DWORD process_id,
    bool optimize
) {
    BatchReadResult result;
    
    if (requests.empty()) {
        return result;
    }
    
    // Validate process ID
    if (!Validation::ProcessValidator::is_valid_process_id(process_id)) {
        for (auto& req : requests) {
            req.success = false;
            req.error_message = "Invalid process ID";
            result.failed_indices.push_back(&req - &requests[0]);
        }
        result.failed_reads = requests.size();
        return result;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Apply optimizations if enabled
    if (optimize) {
        optimize_requests(requests);
    }
    
    // Get DMA instance (would need to be injected or accessed globally)
    // For now, we'll simulate the reads
    // In real implementation, this would use the actual DMA hardware
    
    // Process each request
    for (size_t i = 0; i < requests.size(); ++i) {
        auto& req = requests[i];
        
        // Validate address and size
        if (!Validation::MemoryRangeValidator::is_safe_range(req.address, req.size)) {
            req.success = false;
            req.error_message = "Invalid memory range";
            req.bytes_read = 0;
            result.failed_reads++;
            result.failed_indices.push_back(i);
            continue;
        }
        
        // Allocate destination buffer if not provided
        std::vector<uint8_t> temp_buffer;
        void* dest = req.destination;

        if (dest == nullptr) {
            temp_buffer.resize(req.size);
            dest = temp_buffer.data();
        }

        // Attempt to read from DMA
        bool read_success = false;
        DWORD bytes_read = 0;

        if (pimpl_->vmm_handle) {
            typedef int (*VMMDLL_MemReadEx_t)(void*, DWORD, uint64_t, unsigned char*, DWORD, DWORD*, uint64_t);

            // Get VMMDLL_MemReadEx function from vmmdll.dll
            static HMODULE vmm_dll = GetModuleHandleA("vmm.dll");
            static VMMDLL_MemReadEx_t vmm_read = nullptr;

            if (vmm_dll && !vmm_read) {
                vmm_read = reinterpret_cast<VMMDLL_MemReadEx_t>(
                    GetProcAddress(vmm_dll, "VMMDLL_MemReadEx")
                );
            }

            if (vmm_read) {
                int success = vmm_read(
                    pimpl_->vmm_handle,
                    process_id,
                    req.address,
                    reinterpret_cast<unsigned char*>(dest),
                    static_cast<DWORD>(req.size),
                    &bytes_read,
                    0x00080000 | 0x00010000  // VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL
                );
                read_success = (success != 0 && bytes_read == req.size);
            }
        }

        if (read_success) {
            req.success = true;
            req.bytes_read = req.size;
            result.successful_reads++;
            result.total_bytes_read += req.size;
        } else {
            req.success = false;
            req.error_message = "DMA read failed";
            req.bytes_read = 0;
            result.failed_reads++;
            result.failed_indices.push_back(i);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time
    );
    
    // Calculate throughput
    if (result.duration.count() > 0) {
        double seconds = result.duration.count() / 1000000.0;
        double mb = result.total_bytes_read / (1024.0 * 1024.0);
        result.throughput_mbps = mb / seconds;
    }
    
    // Update statistics
    if (pimpl_->stats_enabled) {
        pimpl_->stats.total_batch_operations++;
        pimpl_->stats.total_individual_reads += requests.size();
        pimpl_->stats.total_successful_reads += result.successful_reads;
        pimpl_->stats.total_failed_reads += result.failed_reads;
        pimpl_->stats.total_bytes_read += result.total_bytes_read;
        pimpl_->stats.total_duration += result.duration;
    }
    
    return result;
}

std::vector<uint8_t> BatchOperations::batch_read_range(
    uint64_t start_address,
    uint64_t end_address,
    size_t chunk_size,
    DWORD process_id
) {
    std::vector<uint8_t> result;
    
    if (end_address <= start_address) {
        return result;
    }
    
    if (chunk_size == 0) {
        chunk_size = 4096;  // Default to 4KB chunks
    }
    
    uint64_t total_size = end_address - start_address;
    result.reserve(total_size);
    
    // Create read requests for each chunk
    std::vector<ReadRequest> requests;
    uint64_t current_address = start_address;
    
    while (current_address < end_address) {
        size_t read_size = static_cast<size_t>(
            std::min<uint64_t>(chunk_size, end_address - current_address)
        );
        
        requests.emplace_back(current_address, read_size);
        current_address += read_size;
    }
    
    // Allocate buffers for each chunk
    std::vector<std::vector<uint8_t>> chunk_buffers(requests.size());
    for (size_t i = 0; i < requests.size(); ++i) {
        chunk_buffers[i].resize(requests[i].size);
        requests[i].destination = chunk_buffers[i].data();
    }
    
    // Execute batch read
    auto batch_result = batch_read(requests, process_id, true);
    
    // Combine successful chunks into result
    for (size_t i = 0; i < requests.size(); ++i) {
        if (requests[i].success) {
            result.insert(result.end(), 
                         chunk_buffers[i].begin(), 
                         chunk_buffers[i].begin() + requests[i].bytes_read);
        } else {
            // Insert zeros for failed chunks
            result.insert(result.end(), requests[i].size, 0);
        }
    }
    
    return result;
}

void BatchOperations::optimize_requests(std::vector<ReadRequest>& requests) {
    if (requests.empty()) {
        return;
    }
    
    // Sort requests by address for better memory locality
    sort_by_address(requests);
    
    if (pimpl_->stats_enabled) {
        pimpl_->stats.addresses_sorted++;
    }
    
    // Group requests by memory page (future optimization)
    // This would be more beneficial with actual hardware DMA
    group_by_page(requests);
}

void BatchOperations::sort_by_address(std::vector<ReadRequest>& requests) {
    std::sort(requests.begin(), requests.end(),
        [](const ReadRequest& a, const ReadRequest& b) {
            return a.address < b.address;
        });
}

void BatchOperations::group_by_page(std::vector<ReadRequest>& requests) {
    // Group reads that fall within the same 4KB page
    // This is a placeholder for more advanced grouping logic
    
    constexpr uint64_t PAGE_SIZE = 4096;
    std::map<uint64_t, std::vector<size_t>> page_groups;
    
    for (size_t i = 0; i < requests.size(); ++i) {
        uint64_t page = requests[i].address / PAGE_SIZE;
        page_groups[page].push_back(i);
    }
    
    if (pimpl_->stats_enabled) {
        pimpl_->stats.pages_grouped += page_groups.size();
    }
    
    // Note: Actual regrouping of requests would happen here
    // For now, we just track the statistics
}

BatchStatistics BatchOperations::get_statistics() const {
    return pimpl_->stats;
}

void BatchOperations::reset_statistics() {
    pimpl_->stats = BatchStatistics();
}

void BatchOperations::set_statistics_enabled(bool enabled) {
    pimpl_->stats_enabled = enabled;
}

bool BatchOperations::is_statistics_enabled() const {
    return pimpl_->stats_enabled;
}

} // namespace VolkDMA
