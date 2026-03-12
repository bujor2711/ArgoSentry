// ArgoSentry ConsoleSink Implementation
// Console logging with color support

#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Include log_sinks.hh first (doesn't include Windows.h)
#include <ArgoSentry/log_sinks.hh>

// Undefine ERROR macro before using our enum
#ifdef ERROR
#undef ERROR
#endif

// Now include Windows.h
#include <Windows.h>
#include <iostream>

namespace ArgoSentry {

ConsoleSink::ConsoleSink(LogLevel min_level, bool use_colors)
    : min_level_(min_level)
    , use_colors_(use_colors)
{
    // Get console handle
    console_handle_ = GetStdHandle(STD_OUTPUT_HANDLE);

    // Get default console color
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(console_handle_, &info)) {
        default_color_ = info.wAttributes;
    }
    else {
        default_color_ = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
}

void ConsoleSink::write(const LogMessage& msg) {
    if (!enabled_) {
        return;
    }

    std::lock_guard<std::mutex> lock(console_mutex_);

    // Set color based on log level
    if (use_colors_) {
        set_console_color(get_color(msg.level));
    }

    // Write to console
    std::string formatted = msg.format(true);
    std::cout << formatted << std::endl;

    // Reset color
    if (use_colors_) {
        reset_console_color();
    }
}

void ConsoleSink::flush() {
    std::lock_guard<std::mutex> lock(console_mutex_);
    std::cout << std::flush;
}

WORD ConsoleSink::get_color(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:
            // Gray
            return FOREGROUND_INTENSITY;

        case LogLevel::INFO:
            // White (default)
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

        case LogLevel::WARN:
            // Yellow
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

        case LogLevel::ERR:
            // Red
            return FOREGROUND_RED | FOREGROUND_INTENSITY;

        case LogLevel::FATAL:
            // Bright red on dark background
            return FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_RED;

        default:
            return default_color_;
    }
}

void ConsoleSink::set_console_color(WORD color) {
    SetConsoleTextAttribute(console_handle_, color);
}

void ConsoleSink::reset_console_color() {
    SetConsoleTextAttribute(console_handle_, default_color_);
}

} // namespace ArgoSentry
