// ArgoSentry Logger Implementation
// Thread-safe async logger with sink management

#define NOMINMAX
#include <ArgoSentry/logger.hh>
#include <algorithm>

namespace ArgoSentry {

Logger::Logger()
    : async_enabled_(true)
    , shutdown_(false)
{
    // Start background worker thread
    worker_ = std::thread(&Logger::worker_thread, this);
}

Logger::~Logger() {
    // Signal shutdown
    shutdown_.store(true, std::memory_order_release);

    // Wake up worker thread
    queue_cv_.notify_all();

    // Wait for worker to finish
    if (worker_.joinable()) {
        worker_.join();
    }

    // Flush all sinks
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    for (auto& sink : sinks_) {
        if (sink->is_enabled()) {
            sink->flush();
        }
    }
}

void Logger::add_sink(std::unique_ptr<ILogSink> sink) {
    if (!sink) {
        return;
    }

    std::lock_guard<std::mutex> lock(sinks_mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    sinks_.clear();
}

void Logger::log(
    LogLevel level,
    const std::string& message,
    const char* file,
    int line,
    const char* function
) {
    // Check minimum level (fast path)
    if (static_cast<int>(level) < min_level_.load(std::memory_order_relaxed)) {
        return;
    }

    // Create log message
    LogMessage msg(
        level,
        message,
        file ? file : "",
        line,
        function ? function : ""
    );

    // Increment message count
    message_count_.fetch_add(1, std::memory_order_relaxed);

    if (async_enabled_) {
        // Async path: Add to queue
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Check if queue is full
        if (message_queue_.size() >= MAX_QUEUE_SIZE) {
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
            return;  // Drop message
        }

        message_queue_.push(std::move(msg));
        lock.unlock();

        // Notify worker thread
        queue_cv_.notify_one();
    }
    else {
        // Sync path: Write immediately
        write_to_sinks(msg);
    }
}

void Logger::flush() {
    if (async_enabled_) {
        // Wait for queue to be empty
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (message_queue_.empty()) {
                break;
            }
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Flush all sinks
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    for (auto& sink : sinks_) {
        if (sink->is_enabled()) {
            sink->flush();
        }
    }
}

void Logger::worker_thread() {
    while (!shutdown_.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Wait for messages or shutdown
        queue_cv_.wait(lock, [this] {
            return !message_queue_.empty() || shutdown_.load(std::memory_order_acquire);
        });

        // Process all messages in queue
        while (!message_queue_.empty()) {
            LogMessage msg = std::move(message_queue_.front());
            message_queue_.pop();
            lock.unlock();

            // Write to sinks (outside lock)
            write_to_sinks(msg);

            lock.lock();
        }
    }

    // Process remaining messages before exit
    std::unique_lock<std::mutex> lock(queue_mutex_);
    while (!message_queue_.empty()) {
        LogMessage msg = std::move(message_queue_.front());
        message_queue_.pop();
        lock.unlock();

        write_to_sinks(msg);

        lock.lock();
    }
}

void Logger::write_to_sinks(const LogMessage& msg) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);

    for (auto& sink : sinks_) {
        if (!sink->is_enabled()) {
            continue;
        }

        // Check sink's minimum level
        if (static_cast<int>(msg.level) < static_cast<int>(sink->get_min_level())) {
            continue;
        }

        try {
            sink->write(msg);
        }
        catch (const std::exception&) {
            // Swallow exceptions from sinks to prevent logger crashes
            // In production, we might want to disable the failing sink
        }
    }
}

} // namespace ArgoSentry
