// ArgoSentry Logging Framework
// Thread-safe async logger with multiple sinks
// Minimal overhead (<1%) for production deployment

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace ArgoSentry {

/**
 * @brief Log severity levels
 * Ordered from lowest to highest severity
 */
enum class LogLevel {
    DEBUG = 0,    // Verbose debugging information
    INFO = 1,     // General informational messages
    WARN = 2,     // Warning conditions
    ERR = 3,      // Error conditions (renamed from ERROR to avoid Windows macro conflict)
    FATAL = 4     // Fatal errors (program may terminate)
};

/**
 * @brief Convert LogLevel to string
 */
inline const char* to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERR:   return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Log message with metadata
 * Contains all information needed for formatting and filtering
 */
struct LogMessage {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    std::string file;       // Source file (optional)
    int line;               // Line number (optional)
    std::string function;   // Function name (optional)
    std::thread::id thread_id;

    LogMessage(
        LogLevel lvl,
        std::string msg,
        std::string f = "",
        int l = 0,
        std::string fn = ""
    )
        : level(lvl)
        , timestamp(std::chrono::system_clock::now())
        , message(std::move(msg))
        , file(std::move(f))
        , line(l)
        , function(std::move(fn))
        , thread_id(std::this_thread::get_id())
    {}

    /**
     * @brief Format message to string
     * @param include_metadata Include timestamp, level, thread ID
     * @return Formatted log string
     */
    [[nodiscard]] std::string format(bool include_metadata = true) const {
        std::ostringstream oss;

        if (include_metadata) {
            // Timestamp
            auto time_t_now = std::chrono::system_clock::to_time_t(timestamp);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()
            ) % 1000;

            std::tm tm_buf{};
            localtime_s(&tm_buf, &time_t_now);

            oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

            // Level
            oss << " [" << std::setw(5) << to_string(level) << "]";

            // Thread ID
            oss << " [T:" << thread_id << "]";

            oss << " ";
        }

        // Message
        oss << message;

        // Source location (if available)
        if (!file.empty() && line > 0) {
            oss << " (" << file << ":" << line << ")";
        }

        return oss.str();
    }
};

/**
 * @brief Abstract log sink interface
 * Implementations can write to file, console, network, etc.
 */
class ILogSink {
public:
    virtual ~ILogSink() = default;

    /**
     * @brief Write log message to sink
     * @param msg Log message to write
     * @note Must be thread-safe
     */
    virtual void write(const LogMessage& msg) = 0;

    /**
     * @brief Flush buffered logs
     */
    virtual void flush() = 0;

    /**
     * @brief Check if sink is enabled
     */
    [[nodiscard]] virtual bool is_enabled() const = 0;

    /**
     * @brief Get minimum log level for this sink
     */
    [[nodiscard]] virtual LogLevel get_min_level() const = 0;
};

/**
 * @brief Thread-safe async logger with multiple sinks
 * 
 * Features:
 * - Thread-safe concurrent logging
 * - Async I/O (background thread)
 * - Multiple sink support (file, console, etc.)
 * - Configurable log levels
 * - Minimal overhead (<1%)
 * - Automatic shutdown
 * 
 * Usage:
 * @code
 * auto logger = Logger::create();
 * logger->add_sink(std::make_unique<FileSink>("app.log"));
 * logger->add_sink(std::make_unique<ConsoleSink>());
 * 
 * logger->log(LogLevel::INFO, "Application started");
 * logger->log(LogLevel::ERROR, "Error occurred", __FILE__, __LINE__);
 * @endcode
 */
class Logger {
public:
    /**
     * @brief Create logger instance
     * @return Unique pointer to logger
     */
    static std::unique_ptr<Logger> create() {
        return std::unique_ptr<Logger>(new Logger());
    }

    /**
     * @brief Destructor - stops background thread and flushes logs
     */
    ~Logger();

    // Non-copyable, non-movable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief Add log sink
     * @param sink Unique pointer to sink implementation
     */
    void add_sink(std::unique_ptr<ILogSink> sink);

    /**
     * @brief Remove all sinks
     */
    void clear_sinks();

    /**
     * @brief Log message with level
     * @param level Log severity level
     * @param message Log message
     * @param file Source file (optional)
     * @param line Line number (optional)
     * @param function Function name (optional)
     */
    void log(
        LogLevel level,
        const std::string& message,
        const char* file = "",
        int line = 0,
        const char* function = ""
    );

    /**
     * @brief Log with format string (printf-style)
     * @param level Log severity level
     * @param format Format string
     * @param args Format arguments
     */
    template<typename... Args>
    void log_fmt(LogLevel level, const char* format, Args&&... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
        log(level, std::string(buffer));
    }

    /**
     * @brief Set minimum log level
     * Messages below this level will be discarded
     * @param level Minimum log level
     */
    void set_min_level(LogLevel level) {
        min_level_.store(static_cast<int>(level), std::memory_order_relaxed);
    }

    /**
     * @brief Get minimum log level
     */
    [[nodiscard]] LogLevel get_min_level() const {
        return static_cast<LogLevel>(min_level_.load(std::memory_order_relaxed));
    }

    /**
     * @brief Enable/disable async logging
     * @param enabled True to enable background thread
     * @note Sync mode has higher overhead but guarantees immediate writes
     */
    void set_async(bool enabled) {
        async_enabled_ = enabled;
    }

    /**
     * @brief Check if async logging is enabled
     */
    [[nodiscard]] bool is_async() const {
        return async_enabled_;
    }

    /**
     * @brief Flush all sinks
     * Blocks until all pending messages are written
     */
    void flush();

    /**
     * @brief Get total messages logged
     */
    [[nodiscard]] size_t get_message_count() const {
        return message_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get messages dropped (queue full)
     */
    [[nodiscard]] size_t get_dropped_count() const {
        return dropped_count_.load(std::memory_order_relaxed);
    }

    // Convenience methods for common log levels
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg) { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg) { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERR, msg); }
    void fatal(const std::string& msg) { log(LogLevel::FATAL, msg); }

private:
    Logger();

    /**
     * @brief Background thread function
     * Processes log queue and writes to sinks
     */
    void worker_thread();

    /**
     * @brief Write message to all sinks
     * @param msg Log message
     */
    void write_to_sinks(const LogMessage& msg);

    // Configuration
    std::atomic<int> min_level_{static_cast<int>(LogLevel::INFO)};
    bool async_enabled_{true};

    // Sinks
    std::vector<std::unique_ptr<ILogSink>> sinks_;
    mutable std::mutex sinks_mutex_;

    // Async queue
    std::queue<LogMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    static constexpr size_t MAX_QUEUE_SIZE = 10000;

    // Worker thread
    std::thread worker_;
    std::atomic<bool> shutdown_{false};

    // Statistics
    std::atomic<size_t> message_count_{0};
    std::atomic<size_t> dropped_count_{0};
};

} // namespace ArgoSentry

// Convenience macros for logging with source location
#define LOG_DEBUG(logger, msg) (logger)->log(ArgoSentry::LogLevel::DEBUG, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(logger, msg)  (logger)->log(ArgoSentry::LogLevel::INFO, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARN(logger, msg)  (logger)->log(ArgoSentry::LogLevel::WARN, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(logger, msg) (logger)->log(ArgoSentry::LogLevel::ERR, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL(logger, msg) (logger)->log(ArgoSentry::LogLevel::FATAL, msg, __FILE__, __LINE__, __FUNCTION__)
