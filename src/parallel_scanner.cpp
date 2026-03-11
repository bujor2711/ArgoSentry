#include "ArgoSentry/parallel_scanner.hh"
#include "ArgoSentry/dma.hh"
#include "ArgoSentry/compiled_pattern.hh"  // v2.5
#include <algorithm>
#include <iostream>

namespace ArgoSentry
{

ParallelScanner::ParallelScanner(DMA& dma, size_t num_threads)
    : dma_(dma)
    , thread_count_(num_threads == 0 ? std::thread::hardware_concurrency() : num_threads)
{
    if (thread_count_ == 0) {
        throw std::runtime_error("No threads available for parallel scanning (hardware_concurrency returned 0)");
    }

    // Clamp to reasonable limits (1-32 threads)
    if (thread_count_ > 32) {
        thread_count_ = 32;
    }
}

ParallelScanner::~ParallelScanner()
{
    // No explicit cleanup needed - std::async handles thread lifetime
    // If any scans are still running, they will complete naturally
}

ScanResult ParallelScanner::find_signature_parallel(
    const char* signature,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id,
    size_t num_threads
)
{
    if (num_threads == 0) {
        num_threads = thread_count_;
    }
    if (num_threads > thread_count_) {
        num_threads = thread_count_;
    }

    // Validate inputs
    if (!signature || signature[0] == '\0') {
        return ScanResult{
            std::nullopt,
            std::make_error_code(std::errc::invalid_argument),
            "Invalid signature: null or empty"
        };
    }

    if (range_start >= range_end) {
        return ScanResult{
            std::nullopt,
            std::make_error_code(std::errc::invalid_argument),
            "Invalid range: start >= end"
        };
    }

    try {
        // Calculate range size
        uint64_t range_size = range_end - range_start;

        // For small ranges, use single-threaded scanning (overhead not worth it)
        if (range_size < MIN_CHUNK_SIZE || num_threads == 1) {
            uint64_t addr = dma_.find_signature(signature, range_start, range_end, process_id);
            return ScanResult{addr, {}, ""};
        }

        // Calculate chunk size - ensure minimum chunk size
        uint64_t chunk_size = range_size / num_threads;
        if (chunk_size < MIN_CHUNK_SIZE) {
            // Reduce thread count to maintain minimum chunk size
            num_threads = range_size / MIN_CHUNK_SIZE;
            if (num_threads == 0) {
                num_threads = 1;
            }
            chunk_size = range_size / num_threads;
        }

        // Reset cancellation flag for this operation
        cancel_flag_.store(false, std::memory_order_relaxed);

        // Launch parallel workers using std::async
        std::vector<std::future<ScanResult>> futures;
        futures.reserve(num_threads);

        for (size_t i = 0; i < num_threads; ++i) {
            // Calculate chunk boundaries
            uint64_t chunk_start = range_start + (i * chunk_size);
            uint64_t chunk_end = (i == num_threads - 1)
                ? range_end  // Last chunk takes remainder
                : chunk_start + chunk_size;

            // Ensure we don't exceed range_end
            if (chunk_end > range_end) {
                chunk_end = range_end;
            }

            // Skip empty chunks
            if (chunk_start >= chunk_end) {
                continue;
            }

            // Launch async task
            futures.push_back(std::async(std::launch::async,
                [this, signature, chunk_start, chunk_end, process_id]() -> ScanResult {
                    try {
                        // Check cancellation flag
                        if (cancel_flag_.load(std::memory_order_relaxed)) {
                            return ScanResult{
                                std::nullopt,
                                std::make_error_code(std::errc::operation_canceled),
                                "Scan cancelled by user"
                            };
                        }

                        // Perform signature scan on this chunk
                        uint64_t addr = dma_.find_signature(signature, chunk_start, chunk_end, process_id);

                        // Check cancellation after scan (for early return)
                        if (cancel_flag_.load(std::memory_order_relaxed)) {
                            return ScanResult{
                                std::nullopt,
                                std::make_error_code(std::errc::operation_canceled),
                                "Scan cancelled by user"
                            };
                        }

                        return ScanResult{addr, {}, ""};

                    } catch (const std::exception& e) {
                        return ScanResult{
                            std::nullopt,
                            std::make_error_code(std::errc::io_error),
                            std::string("DMA error in worker thread: ") + e.what()
                        };
                    } catch (...) {
                        return ScanResult{
                            std::nullopt,
                            std::make_error_code(std::errc::io_error),
                            "Unknown error in worker thread"
                        };
                    }
                }
            ));
        }

        // Collect results - return first match found
        for (auto& future : futures) {
            try {
                ScanResult result = future.get();

                // If error occurred, propagate it
                if (!result.success()) {
                    return result;
                }

                // If pattern found, cancel other threads and return immediately
                if (result.found()) {
                    cancel_flag_.store(true, std::memory_order_relaxed);
                    return result;
                }

            } catch (const std::exception& e) {
                return ScanResult{
                    std::nullopt,
                    std::make_error_code(std::errc::io_error),
                    std::string("Failed to get worker result: ") + e.what()
                };
            }
        }

        // Pattern not found in any chunk
        return ScanResult{0, {}, ""};

    } catch (const std::bad_alloc&) {
        return ScanResult{
            std::nullopt,
            std::make_error_code(std::errc::not_enough_memory),
            "Memory allocation failed for parallel scan"
        };
    } catch (const std::exception& e) {
        return ScanResult{
            std::nullopt,
            std::make_error_code(std::errc::io_error),
            std::string("Parallel scan error: ") + e.what()
        };
    }
}

std::future<ScanResult> ParallelScanner::find_signature_async(
    const char* signature,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id
)
{
    // Launch the parallel scan asynchronously
    return std::async(std::launch::async,
        [this, signature, range_start, range_end, process_id]() -> ScanResult {
            return find_signature_parallel(signature, range_start, range_end, process_id);
        }
    );
}

// Overload for CompiledPattern (v2.5 - combines compiled + parallel benefits)
ScanResult ParallelScanner::find_signature_parallel(
    const CompiledPattern& pattern,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id,
    size_t num_threads
)
{
    try {
        // Reset cancellation flag
        reset_cancel();

        // Determine actual thread count
        size_t actual_threads = (num_threads == 0) ? thread_count_ : num_threads;

        // For small ranges, fallback to single-threaded compiled pattern scan
        constexpr uint64_t MIN_PARALLEL_RANGE = 4096; // 4KB
        if (range_end - range_start < MIN_PARALLEL_RANGE * actual_threads) {
            uint64_t addr = dma_.find_signature(pattern, range_start, range_end, process_id);
            return ScanResult{addr, {}, ""};
        }

        // Split range into chunks (one per thread)
        uint64_t range_size = range_end - range_start;
        uint64_t chunk_size = range_size / actual_threads;

        // Launch worker threads
        std::vector<std::future<ScanResult>> futures;
        futures.reserve(actual_threads);

        for (size_t i = 0; i < actual_threads; ++i) {
            uint64_t chunk_start = range_start + (i * chunk_size);
            uint64_t chunk_end = (i == actual_threads - 1) ? range_end : (chunk_start + chunk_size);

            futures.push_back(std::async(std::launch::async,
                [this, pattern, chunk_start, chunk_end, process_id]() -> ScanResult {
                    try {
                        // Check cancellation before scanning
                        if (cancel_flag_.load(std::memory_order_relaxed)) {
                            return ScanResult{0, {}, ""};
                        }

                        // Use DMA's compiled pattern scan
                        uint64_t addr = dma_.find_signature(pattern, chunk_start, chunk_end, process_id);
                        return ScanResult{addr, {}, ""};

                    } catch (const std::exception& e) {
                        return ScanResult{
                            std::nullopt,
                            std::make_error_code(std::errc::io_error),
                            std::string("Worker error: ") + e.what()
                        };
                    } catch (...) {
                        return ScanResult{
                            std::nullopt,
                            std::make_error_code(std::errc::io_error),
                            "Unknown error in worker thread"
                        };
                    }
                }
            ));
        }

        // Collect results - return first match found
        for (auto& future : futures) {
            try {
                ScanResult result = future.get();

                // If error occurred, propagate it
                if (!result.success()) {
                    return result;
                }

                // If pattern found, cancel other threads and return immediately
                if (result.found()) {
                    cancel_flag_.store(true, std::memory_order_relaxed);
                    return result;
                }

            } catch (const std::exception& e) {
                return ScanResult{
                    std::nullopt,
                    std::make_error_code(std::errc::io_error),
                    std::string("Failed to get worker result: ") + e.what()
                };
            }
        }

        // Pattern not found in any chunk
        return ScanResult{0, {}, ""};

    } catch (const std::bad_alloc&) {
        return ScanResult{
            std::nullopt,
            std::make_error_code(std::errc::not_enough_memory),
            "Memory allocation failed for parallel scan"
        };
    } catch (const std::exception& e) {
        return ScanResult{
            std::nullopt,
            std::make_error_code(std::errc::io_error),
            std::string("Parallel scan error: ") + e.what()
        };
    }
}

// Async version with CompiledPattern (v2.5)
std::future<ScanResult> ParallelScanner::find_signature_async(
    const CompiledPattern& pattern,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id
)
{
    // Launch the parallel scan asynchronously with compiled pattern
    return std::async(std::launch::async,
        [this, pattern, range_start, range_end, process_id]() -> ScanResult {
            return find_signature_parallel(pattern, range_start, range_end, process_id);
        }
    );
}

void ParallelScanner::cancel()
{
    cancel_flag_.store(true, std::memory_order_relaxed);
}

void ParallelScanner::reset_cancel()
{
    cancel_flag_.store(false, std::memory_order_relaxed);
}

} // namespace ArgoSentry
