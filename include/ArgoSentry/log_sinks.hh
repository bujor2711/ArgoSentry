// ArgoSentry Log Sink Implementations
// File, console, and custom sink implementations

#pragma once

#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "logger.hh"
#include <fstream>
#include <filesystem>

// Forward declare Windows types to avoid including Windows.h in header
using WORD = unsigned short;
using HANDLE = void*;

namespace ArgoSentry {

/**
 * @brief File sink with rotation support
 * 
 * Features:
 * - Size-based rotation (e.g., rotate at 10MB)
 * - Time-based rotation (e.g., daily rotation)
 * - Configurable max file count
 * - Automatic compression (optional)
 * - Thread-safe file writes
 * 
 * File naming: app.log, app.1.log, app.2.log, etc.
 */
class FileSink : public ILogSink {
public:
    /**
     * @brief Rotation policy
     */
    enum class RotationPolicy {
        NONE,       // No rotation
        SIZE,       // Rotate when file reaches max size
        DAILY,      // Rotate daily at midnight
        HOURLY      // Rotate every hour
    };

    /**
     * @brief Create file sink
     * @param filepath Path to log file
     * @param min_level Minimum log level to write
     * @param policy Rotation policy
     * @param max_size Max file size in bytes (for SIZE policy)
     * @param max_files Max number of rotated files to keep
     */
    FileSink(
        const std::filesystem::path& filepath,
        LogLevel min_level = LogLevel::INFO,
        RotationPolicy policy = RotationPolicy::SIZE,
        size_t max_size = 10 * 1024 * 1024,  // 10MB default
        size_t max_files = 5                  // Keep 5 rotated files
    );

    ~FileSink() override;

    void write(const LogMessage& msg) override;
    void flush() override;

    [[nodiscard]] bool is_enabled() const override { return enabled_; }
    [[nodiscard]] LogLevel get_min_level() const override { return min_level_; }

    /**
     * @brief Enable/disable sink
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Get current file size
     */
    [[nodiscard]] size_t get_current_size() const;

    /**
     * @brief Force rotation now
     */
    void rotate();

private:
    /**
     * @brief Check if rotation is needed
     */
    bool should_rotate() const;

    /**
     * @brief Perform rotation (rename files)
     */
    void perform_rotation();

    /**
     * @brief Open log file
     */
    void open_file();

    /**
     * @brief Close log file
     */
    void close_file();

    std::filesystem::path filepath_;
    std::ofstream file_;
    mutable std::mutex file_mutex_;

    LogLevel min_level_;
    RotationPolicy policy_;
    size_t max_size_;
    size_t max_files_;
    bool enabled_{true};

    size_t current_size_{0};
    std::chrono::system_clock::time_point last_rotation_;
};

/**
 * @brief Console sink with color support
 * 
 * Features:
 * - Colored output (ERROR=red, WARN=yellow, etc.)
 * - Windows console API support
 * - Thread-safe console writes
 * - Configurable format
 */
class ConsoleSink : public ILogSink {
public:
    /**
     * @brief Create console sink
     * @param min_level Minimum log level to write
     * @param use_colors Enable colored output
     */
    explicit ConsoleSink(
        LogLevel min_level = LogLevel::INFO,
        bool use_colors = true
    );

    ~ConsoleSink() override = default;

    void write(const LogMessage& msg) override;
    void flush() override;

    [[nodiscard]] bool is_enabled() const override { return enabled_; }
    [[nodiscard]] LogLevel get_min_level() const override { return min_level_; }

    /**
     * @brief Enable/disable sink
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }

    /**
     * @brief Enable/disable colors
     */
    void set_colors(bool use_colors) { use_colors_ = use_colors; }

private:
    /**
     * @brief Get console color for log level
     */
    WORD get_color(LogLevel level) const;

    /**
     * @brief Set console color
     */
    void set_console_color(WORD color);

    /**
     * @brief Reset console color to default
     */
    void reset_console_color();

    LogLevel min_level_;
    bool use_colors_;
    bool enabled_{true};

    HANDLE console_handle_;
    WORD default_color_;
    mutable std::mutex console_mutex_;
};

/**
 * @brief Memory sink for testing
 * Stores log messages in memory for inspection
 */
class MemorySink : public ILogSink {
public:
    explicit MemorySink(LogLevel min_level = LogLevel::DEBUG)
        : min_level_(min_level)
    {}

    void write(const LogMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.push_back(msg);
    }

    void flush() override {
        // No-op for memory sink
    }

    [[nodiscard]] bool is_enabled() const override { return true; }
    [[nodiscard]] LogLevel get_min_level() const override { return min_level_; }

    /**
     * @brief Get all stored messages
     */
    [[nodiscard]] std::vector<LogMessage> get_messages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }

    /**
     * @brief Clear stored messages
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.clear();
    }

    /**
     * @brief Get message count
     */
    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_.size();
    }

private:
    LogLevel min_level_;
    std::vector<LogMessage> messages_;
    mutable std::mutex mutex_;
};

} // namespace ArgoSentry
