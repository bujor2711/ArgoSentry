// ArgoSentry/async.hh - Async Operations for Multi-Core Performance
// v2.3 - Provides 2-4x speedup through parallelization
#pragma once

#include <ArgoSentry/dma.hh>  // Need complete DMA definition for templates
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <Windows.h>

namespace ArgoSentry {

// DMA class forward declaration (already included above)
// class DMA;

namespace Async {

//==============================================================================
// Progress Callback
//==============================================================================
using ProgressCallback = std::function<void(size_t current, size_t total, const std::string& status)>;

//==============================================================================
// Async Result with Cancellation Support
//==============================================================================
template<typename T>
class AsyncResult {
private:
    std::future<T> future_;
    std::shared_ptr<std::atomic<bool>> cancel_flag_;
    
public:
    explicit AsyncResult(std::future<T>&& fut, std::shared_ptr<std::atomic<bool>> cancel)
        : future_(std::move(fut)), cancel_flag_(cancel) {}
    
    // Get result (blocking)
    T get() { return future_.get(); }
    
    // Check if ready
    bool is_ready() const {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    
    // Cancel operation
    void cancel() {
        if (cancel_flag_) {
            cancel_flag_->store(true);
        }
    }
    
    // Check if cancelled
    bool is_cancelled() const {
        return cancel_flag_ && cancel_flag_->load();
    }
};

//==============================================================================
// Thread Pool for DMA Operations
//==============================================================================
class DMAThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_tasks_;
    
public:
    // Constructor with optional thread count (default: hardware concurrency)
    explicit DMAThreadPool(size_t thread_count = 0);
    
    // Destructor - waits for all tasks to complete
    ~DMAThreadPool();
    
    // Disable copy/move
    DMAThreadPool(const DMAThreadPool&) = delete;
    DMAThreadPool& operator=(const DMAThreadPool&) = delete;
    
    // Enqueue a task and get future
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // Wait for all tasks to complete
    void wait_all();
    
    // Get queue size
    size_t get_queue_size() const;
    
    // Get active task count
    size_t get_active_count() const { return active_tasks_.load(); }
    
    // Get thread count
    size_t get_thread_count() const { return workers_.size(); }
};

//==============================================================================
// Async Signature Scanning
//==============================================================================

// Async signature scan - returns future with address
std::future<uint64_t> find_signature_async(
    const DMA& dma,
    const char* signature,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id
);

// Async signature scan with cancellation
AsyncResult<uint64_t> find_signature_async_cancellable(
    const DMA& dma,
    const char* signature,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id
);

// Async signature scan with progress callback
std::future<uint64_t> find_signature_async_with_progress(
    const DMA& dma,
    const char* signature,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id,
    ProgressCallback callback,
    size_t update_interval_ms = 100
);

//==============================================================================
// Parallel Memory Operations
//==============================================================================

// Parallel read multiple addresses
std::vector<std::future<std::vector<uint8_t>>> read_multiple_async(
    const DMA& dma,
    const std::vector<uint64_t>& addresses,
    size_t size_per_address,
    DWORD process_id
);

// Parallel read with typed results
template<typename T>
std::vector<std::future<T>> read_multiple_typed_async(
    const DMA& dma,
    const std::vector<uint64_t>& addresses,
    DWORD process_id
);

//==============================================================================
// Parallel Signature Scanning
//==============================================================================

// Scan multiple patterns in parallel
struct PatternMatch {
    const char* pattern;
    uint64_t address;
    bool found;
};

std::vector<std::future<PatternMatch>> find_signatures_parallel(
    const DMA& dma,
    const std::vector<const char*>& patterns,
    uint64_t range_start,
    uint64_t range_end,
    DWORD process_id
);

// Scan single pattern across multiple memory regions in parallel
std::future<uint64_t> find_signature_parallel_regions(
    const DMA& dma,
    const char* signature,
    const std::vector<std::pair<uint64_t, uint64_t>>& regions,
    DWORD process_id
);

//==============================================================================
// Template Implementations
//==============================================================================

template<typename F, typename... Args>
auto DMAThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
        }
        tasks_.emplace([task, this]() {
            active_tasks_++;
            (*task)();
            active_tasks_--;
        });
    }
    
    condition_.notify_one();
    return result;
}

template<typename T>
std::vector<std::future<T>> read_multiple_typed_async(
    const DMA& dma,
    const std::vector<uint64_t>& addresses,
    DWORD process_id
)
{
    std::vector<std::future<T>> results;
    results.reserve(addresses.size());

    for (const auto& addr : addresses) {
        results.push_back(std::async(std::launch::async, 
            [&dma, addr, process_id]() -> T {
                return dma.template read<T>(addr, process_id);
            }
        ));
    }

    return results;
}

} // namespace Async
} // namespace ArgoSentry

