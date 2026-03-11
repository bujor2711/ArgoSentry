#ifndef ARGOSENTRY_PATTERN_LIBRARY_HH
#define ARGOSENTRY_PATTERN_LIBRARY_HH

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <filesystem>
#include <shared_mutex>

namespace ArgoSentry {

/**
 * @brief Pattern entry with metadata
 */
struct PatternEntry {
    std::string name;
    std::string description;
    std::string pattern;
    std::string game;
    std::string version;
    std::vector<std::string> tags;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Validate pattern entry
     * @return true if entry is valid
     */
    [[nodiscard]] bool is_valid() const;

private:
    /**
     * @brief Validate pattern format
     * @return true if pattern format is correct
     */
    bool validate_pattern() const;
};

/**
 * @brief Error codes for pattern library operations
 */
enum class PatternLibraryError {
    Success,
    FileNotFound,
    FileAccessDenied,
    FileTooLarge,
    ParseError,
    InvalidPattern,
    DuplicateEntry,
    NotFound
};

/**
 * @brief Convert error code to string
 */
[[nodiscard]] const char* to_string(PatternLibraryError error);

/**
 * @brief Pattern library for managing signature patterns
 * 
 * Thread-safe repository for storing, loading, and searching patterns.
 * Supports file I/O, validation, and querying by name, tag, or game.
 * 
 * File format (simple text):
 * # Comment lines start with #
 * name|pattern|description|game|version|tags
 * 
 * Example:
 * player_base|48 8B 0D ? ? ? ?|Player base pointer|cs2.exe|1.2.0|player,base
 */
class PatternLibrary {
public:
    /**
     * @brief Statistics for pattern library
     */
    struct Stats {
        size_t total_patterns{ 0 };
        size_t total_searches{ 0 };
        size_t cache_hits{ 0 };
    };

    PatternLibrary() = default;
    ~PatternLibrary() = default;

    // Non-copyable
    PatternLibrary(const PatternLibrary&) = delete;
    PatternLibrary& operator=(const PatternLibrary&) = delete;

    // Movable
    PatternLibrary(PatternLibrary&&) = default;
    PatternLibrary& operator=(PatternLibrary&&) = default;

    /**
     * @brief Load patterns from file
     * @param filename Path to pattern file
     * @return Error code (Success on success)
     * 
     * File format:
     * # ArgoSentry Pattern Library
     * # Format: name|pattern|description|game|version|tags
     * player_base|48 8B 0D ? ? ? ?|Player base pointer|cs2.exe|1.2.0|player,base
     */
    [[nodiscard]] PatternLibraryError load_from_file(const std::string& filename);

    /**
     * @brief Save patterns to file
     * @param filename Path to output file
     * @return Error code (Success on success)
     */
    [[nodiscard]] PatternLibraryError save_to_file(const std::string& filename) const;

    /**
     * @brief Add or update pattern
     * @param entry Pattern entry to add
     * @return Error code (Success on success)
     */
    [[nodiscard]] PatternLibraryError add_pattern(const PatternEntry& entry);

    /**
     * @brief Remove pattern by name
     * @param name Pattern name to remove
     */
    void remove_pattern(const std::string& name);

    /**
     * @brief Get pattern by name
     * @param name Pattern name
     * @return Pattern entry if found, nullopt otherwise
     */
    [[nodiscard]] std::optional<PatternEntry> get_pattern(const std::string& name) const;

    /**
     * @brief Search patterns by tag
     * @param tag Tag to search for
     * @return Vector of matching patterns
     */
    [[nodiscard]] std::vector<PatternEntry> search_by_tag(const std::string& tag) const;

    /**
     * @brief Search patterns by game
     * @param game Game name to search for
     * @return Vector of matching patterns
     */
    [[nodiscard]] std::vector<PatternEntry> search_by_game(const std::string& game) const;

    /**
     * @brief Get all patterns
     * @return Vector of all patterns
     */
    [[nodiscard]] std::vector<PatternEntry> get_all_patterns() const;

    /**
     * @brief Clear all patterns
     */
    void clear();

    /**
     * @brief Get number of patterns
     * @return Pattern count
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Check if library is empty
     * @return true if no patterns loaded
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Get statistics
     * @return Library statistics
     */
    [[nodiscard]] const Stats& get_stats() const;

private:
    static constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB
    static constexpr size_t MAX_PATTERNS = 10000;

    std::unordered_map<std::string, PatternEntry> patterns_;
    mutable std::shared_mutex mutex_;  // Read-write lock
    std::filesystem::path file_path_;
    mutable Stats stats_;
};

}  // namespace ArgoSentry

#endif  // ARGOSENTRY_PATTERN_LIBRARY_HH
