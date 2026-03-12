// ArgoSentry FileSink Implementation
// File logging with rotation support

#define NOMINMAX
#include <ArgoSentry/log_sinks.hh>
#include <sstream>
#include <iomanip>

namespace ArgoSentry {

FileSink::FileSink(
    const std::filesystem::path& filepath,
    LogLevel min_level,
    RotationPolicy policy,
    size_t max_size,
    size_t max_files
)
    : filepath_(filepath)
    , min_level_(min_level)
    , policy_(policy)
    , max_size_(max_size)
    , max_files_(max_files)
    , last_rotation_(std::chrono::system_clock::now())
{
    // Create directory if it doesn't exist
    if (filepath_.has_parent_path()) {
        std::filesystem::create_directories(filepath_.parent_path());
    }

    open_file();
}

FileSink::~FileSink() {
    close_file();
}

void FileSink::write(const LogMessage& msg) {
    if (!enabled_) {
        return;
    }

    std::lock_guard<std::mutex> lock(file_mutex_);

    // Check if rotation is needed
    if (should_rotate()) {
        perform_rotation();
    }

    if (!file_.is_open()) {
        open_file();
    }

    if (file_.is_open()) {
        std::string formatted = msg.format(true);
        file_ << formatted << '\n';

        current_size_ += formatted.size() + 1;  // +1 for newline
    }
}

void FileSink::flush() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

size_t FileSink::get_current_size() const {
    std::lock_guard<std::mutex> lock(file_mutex_);
    return current_size_;
}

void FileSink::rotate() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    perform_rotation();
}

bool FileSink::should_rotate() const {
    switch (policy_) {
        case RotationPolicy::NONE:
            return false;

        case RotationPolicy::SIZE:
            return current_size_ >= max_size_;

        case RotationPolicy::DAILY: {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            auto last_time_t = std::chrono::system_clock::to_time_t(last_rotation_);

            std::tm now_tm{}, last_tm{};
            localtime_s(&now_tm, &now_time_t);
            localtime_s(&last_tm, &last_time_t);

            // Rotate if day changed
            return now_tm.tm_yday != last_tm.tm_yday ||
                   now_tm.tm_year != last_tm.tm_year;
        }

        case RotationPolicy::HOURLY: {
            auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
                std::chrono::system_clock::now() - last_rotation_
            );
            return elapsed.count() >= 1;
        }

        default:
            return false;
    }
}

void FileSink::perform_rotation() {
    // Close current file
    close_file();

    // Rename existing files
    // app.log -> app.1.log
    // app.1.log -> app.2.log
    // etc.
    std::filesystem::path base = filepath_;
    std::filesystem::path parent = base.parent_path();
    std::string stem = base.stem().string();
    std::string extension = base.extension().string();

    // Delete oldest file if max_files reached
    if (max_files_ > 0) {
        std::filesystem::path oldest = parent / (stem + "." + std::to_string(max_files_) + extension);
        if (std::filesystem::exists(oldest)) {
            std::filesystem::remove(oldest);
        }
    }

    // Rename files (from oldest to newest)
    for (int i = static_cast<int>(max_files_) - 1; i >= 1; --i) {
        std::filesystem::path old_path = parent / (stem + "." + std::to_string(i) + extension);
        std::filesystem::path new_path = parent / (stem + "." + std::to_string(i + 1) + extension);

        if (std::filesystem::exists(old_path)) {
            std::filesystem::rename(old_path, new_path);
        }
    }

    // Rename current file to .1
    if (std::filesystem::exists(filepath_)) {
        std::filesystem::path rotated = parent / (stem + ".1" + extension);
        std::filesystem::rename(filepath_, rotated);
    }

    // Reset state
    current_size_ = 0;
    last_rotation_ = std::chrono::system_clock::now();

    // Open new file
    open_file();
}

void FileSink::open_file() {
    file_.open(filepath_, std::ios::out | std::ios::app);

    if (file_.is_open()) {
        // Get current file size
        file_.seekp(0, std::ios::end);
        current_size_ = static_cast<size_t>(file_.tellp());

        // Write header if new file
        if (current_size_ == 0) {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
            localtime_s(&tm_buf, &time_t_now);

            file_ << "# ArgoSentry Log File\n";
            file_ << "# Started: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "\n";
            file_ << "# ========================================\n\n";
            file_.flush();

            current_size_ = static_cast<size_t>(file_.tellp());
        }
    }
}

void FileSink::close_file() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

} // namespace ArgoSentry
