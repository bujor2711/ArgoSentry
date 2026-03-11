#ifndef ARGOSENTRY_PARALLEL_SCANNER_HH
#define ARGOSENTRY_PARALLEL_SCANNER_HH

#include <cstdint>
#include <optional>
#include <system_error>
#include <string>
#include <future>
#include <atomic>
#include <thread>
#include <vector>
#include <windows.h>

#include "compiled_pattern.hh"  // v2.5 - Compiled pattern support

namespace ArgoSentry
{

class DMA; // Forward declaration

/**
 * @brief Result type for signature scanning with error handling
 * 
 * Encapsulates the result of a signature scan operation, including
 * the found address (if any) and error information if the operation failed.
 */
struct ScanResult
{
    std::optional<uint64_t> address;  ///< Found address, or nullopt if error occurred
    std::error_code error;            ///< Error code if operation failed
    std::string error_message;        ///< Human-readable error description

    /**
     * @brief Check if the scan operation succeeded
     * @return true if scan completed without errors (address may still be 0 if not found)
     */
    [[nodiscard]] bool success() const
    {
        return address.has_value() && !error;
    }

    /**
     * @brief Check if the pattern was found
     * @return true if scan succeeded AND pattern was found (address != 0)
     */
    [[nodiscard]] bool found() const
    {
        return success() && address.value() != 0;
    }
};

/**
 * @brief Parallel signature scanner with thread pool and error handling
 * 
 * Provides parallel signature scanning capabilities for large memory ranges.
 * Uses a thread pool for efficiency and supports cancellation.
 * 
 * Thread-safe: Multiple scans can run concurrently.
 * 
 * @note For small ranges (<4KB), automatically falls back to single-threaded scanning.
 * @note Best performance for ranges >10MB on multi-core CPUs.
 * 
 * Example usage:
 * @code
 * auto dma = DMA::Builder().build();
 * ParallelScanner scanner(*dma, 4);  // 4 threads
 * 
 * auto result = scanner.find_signature_parallel("48 8B 0D ? ? ? ?", start, end, pid);
 * if (result.found()) {
 *     std::cout << "Found at: 0x" << std::hex << result.address.value() << "\n";
 * } else if (!result.success()) {
 *     std::cerr << "Error: " << result.error_message << "\n";
 * }
 * @endcode
 */
class ParallelScanner
{
public:
    /**
     * @brief Construct a parallel scanner
     * @param dma Reference to DMA instance (must outlive this scanner)
     * @param num_threads Number of threads to use (0 = auto-detect)
     * @throws std::runtime_error if no threads available
     */
    explicit ParallelScanner(DMA& dma, size_t num_threads = 0);

    /**
     * @brief Destructor - waits for all threads to complete
     */
    ~ParallelScanner();

    // Non-copyable, non-movable (contains thread pool)
    ParallelScanner(const ParallelScanner&) = delete;
    ParallelScanner& operator=(const ParallelScanner&) = delete;
    ParallelScanner(ParallelScanner&&) = delete;
    ParallelScanner& operator=(ParallelScanner&&) = delete;

    /**
     * @brief Parallel signature scan with error handling
     * 
     * Splits the memory range into chunks and scans them in parallel.
     * Returns as soon as the first match is found.
     * 
     * @param signature Pattern string (e.g., "48 8B 0D ? ? ? ?")
     * @param range_start Start address of scan range
     * @param range_end End address of scan range (exclusive)
     * @param process_id Target process ID
     * @param num_threads Number of threads to use (0 = use default from constructor)
     * 
     * @return ScanResult with address (0 if not found) or error information
     * 
     * @note Thread-safe: Multiple calls can run concurrently
     * @note For ranges <4KB, automatically uses single-threaded scanning
     * 
     * Error codes:
     * - std::errc::operation_canceled: Scan was cancelled
     * - std::errc::io_error: DMA read error or exception occurred
     */
    [[nodiscard]] ScanResult find_signature_parallel(
        const char* signature,
        uint64_t range_start,
        uint64_t range_end,
        DWORD process_id,
        size_t num_threads = 0
    );

    /**
     * @brief Parallel signature scan with compiled pattern (v2.5 - 2-3x faster!)
     * 
     * Same as string version, but uses pre-compiled pattern for speed.
     * Best for patterns reused multiple times (>10 scans).
     * 
     * @param pattern Compiled pattern (created via CompiledPattern::compile())
     * @param range_start Start address of scan range
     * @param range_end End address of scan range (exclusive)
     * @param process_id Target process ID
     * @param num_threads Number of threads to use (0 = use default from constructor)
     * 
     * @return ScanResult with address (0 if not found) or error information
     * 
     * @note 2-3x faster than string version for repeated scans
     * @note Combines benefits of compiled patterns + parallel scanning
     * 
     * Example:
     * @code
     * auto pattern = CompiledPattern::compile("48 8B 0D ? ? ? ?");
     * for (auto& process : processes) {
     *     auto result = scanner.find_signature_parallel(pattern, start, end, process.pid);
     * }
     * @endcode
     */
    [[nodiscard]] ScanResult find_signature_parallel(
        const CompiledPattern& pattern,
        uint64_t range_start,
        uint64_t range_end,
        DWORD process_id,
        size_t num_threads = 0
    );

    /**
     * @brief Async signature scan (non-blocking)
     * 
     * Launches a parallel scan in the background and returns immediately.
     * 
     * @param signature Pattern string
     * @param range_start Start address
     * @param range_end End address (exclusive)
     * @param process_id Target process ID
     * 
     * @return std::future<ScanResult> that will contain the result
     * 
     * Example:
     * @code
     * auto future = scanner.find_signature_async("E8 ? ? ? ?", start, end, pid);
     * // Do other work...
     * auto result = future.get();  // Wait for completion
     * @endcode
     */
    [[nodiscard]] std::future<ScanResult> find_signature_async(
        const char* signature,
        uint64_t range_start,
        uint64_t range_end,
        DWORD process_id
    );

    /**
     * @brief Async signature scan with compiled pattern (v2.5)
     * 
     * @param pattern Compiled pattern
     * @param range_start Start address
     * @param range_end End address (exclusive)
     * @param process_id Target process ID
     * 
     * @return std::future<ScanResult> that will contain the result
     */
    [[nodiscard]] std::future<ScanResult> find_signature_async(
        const CompiledPattern& pattern,
        uint64_t range_start,
        uint64_t range_end,
        DWORD process_id
    );

    /**
     * @brief Cancel all ongoing scan operations
     * 
     * Sets a cancellation flag that all running scans will check.
     * Does not wait for scans to complete - they will return soon with operation_canceled error.
     */
    void cancel();

    /**
     * @brief Reset cancellation flag
     * 
     * Allows new scans to run after cancel() was called.
     */
    void reset_cancel();

    /**
     * @brief Get the number of threads in the pool
     * @return Number of threads available for parallel scanning
     */
    [[nodiscard]] size_t get_thread_count() const { return thread_count_; }

private:
    DMA& dma_;                              ///< Reference to DMA instance
    size_t thread_count_;                   ///< Number of worker threads
    std::atomic<bool> cancel_flag_{false};  ///< Cancellation flag (thread-safe)

    // Minimum chunk size for parallelization (4KB)
    static constexpr size_t MIN_CHUNK_SIZE = 4096;
};

} // namespace ArgoSentry

#endif // ARGOSENTRY_PARALLEL_SCANNER_HH
