#include "ArgoSentry/pattern_library.hh"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ArgoSentry {

// ============================================================================
// PatternEntry Implementation
// ============================================================================

bool PatternEntry::is_valid() const {
    return !name.empty() && !pattern.empty() && validate_pattern();
}

bool PatternEntry::validate_pattern() const {
    // Check pattern format: "48 8B ? ? 0D"
    std::istringstream stream(pattern);
    std::string token;

    while (stream >> token) {
        if (token == "?" || token == "??") {
            // Wildcard - OK
            continue;
        }

        // Must be valid hex (2 characters)
        if (token.size() != 2) {
            return false;
        }

        for (char c : token) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================================
// Error Code to String
// ============================================================================

const char* to_string(PatternLibraryError error) {
    switch (error) {
    case PatternLibraryError::Success:
        return "Success";
    case PatternLibraryError::FileNotFound:
        return "File not found";
    case PatternLibraryError::FileAccessDenied:
        return "File access denied";
    case PatternLibraryError::FileTooLarge:
        return "File too large (>10MB)";
    case PatternLibraryError::ParseError:
        return "Parse error";
    case PatternLibraryError::InvalidPattern:
        return "Invalid pattern format";
    case PatternLibraryError::DuplicateEntry:
        return "Duplicate pattern name";
    case PatternLibraryError::NotFound:
        return "Pattern not found";
    default:
        return "Unknown error";
    }
}

// ============================================================================
// PatternLibrary Implementation
// ============================================================================

PatternLibraryError PatternLibrary::load_from_file(const std::string& filename) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    try {
        // ✅ Path traversal protection: Resolve canonical path
        std::filesystem::path file_path;
        try {
            file_path = std::filesystem::canonical(filename);
        } catch (const std::filesystem::filesystem_error&) {
            // File doesn't exist or cannot be resolved
            return PatternLibraryError::FileNotFound;
        }

        // ✅ Restrict to allowed directories (patterns/ subdirectory or current directory)
        std::filesystem::path current_dir = std::filesystem::current_path();
        std::filesystem::path allowed_dir = current_dir / "patterns";

        std::string file_str = file_path.string();
        std::string current_str = current_dir.string();
        std::string allowed_str = allowed_dir.string();

        // Allow files in current directory or patterns/ subdirectory
        bool in_current = file_str.find(current_str) == 0;
        bool in_patterns = file_str.find(allowed_str) == 0;

        if (!in_current && !in_patterns) {
            // ✅ Reject path traversal attempts
            return PatternLibraryError::FileAccessDenied;
        }

        // Check file exists (redundant but for clarity)
        if (!std::filesystem::exists(file_path)) {
            return PatternLibraryError::FileNotFound;
        }

        // Check file size
        auto file_size = std::filesystem::file_size(file_path);
        if (file_size > MAX_FILE_SIZE) {
            return PatternLibraryError::FileTooLarge;
        }

        // Read file (using validated canonical path)
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return PatternLibraryError::FileAccessDenied;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        // Parse line by line
        std::istringstream stream(content);
        std::string line;

        size_t loaded_count = 0;
        while (std::getline(stream, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Parse format: name|pattern|description|game|version|tags
            std::istringstream line_stream(line);
            std::string name, pattern, desc, game, version, tags_str;

            if (!std::getline(line_stream, name, '|')) continue;
            if (!std::getline(line_stream, pattern, '|')) continue;
            if (!std::getline(line_stream, desc, '|')) continue;
            if (!std::getline(line_stream, game, '|')) continue;
            if (!std::getline(line_stream, version, '|')) continue;
            std::getline(line_stream, tags_str, '|');

            // Create entry
            PatternEntry entry;
            entry.name = name;
            entry.pattern = pattern;
            entry.description = desc;
            entry.game = game;
            entry.version = version;
            entry.created_at = std::chrono::system_clock::now();
            entry.updated_at = entry.created_at;

            // Parse tags (comma-separated)
            std::istringstream tags_stream(tags_str);
            std::string tag;
            while (std::getline(tags_stream, tag, ',')) {
                if (!tag.empty()) {
                    entry.tags.push_back(tag);
                }
            }

            // Validate
            if (!entry.is_valid()) {
                return PatternLibraryError::InvalidPattern;
            }

            // Check for duplicates
            if (patterns_.find(entry.name) != patterns_.end()) {
                return PatternLibraryError::DuplicateEntry;
            }

            // Check limits
            if (patterns_.size() >= MAX_PATTERNS) {
                return PatternLibraryError::FileTooLarge;
            }

            patterns_[entry.name] = std::move(entry);
            loaded_count++;
        }

        file_path_ = filename;
        stats_.total_patterns = patterns_.size();

        return PatternLibraryError::Success;

    }
    catch (const std::exception&) {
        return PatternLibraryError::ParseError;
    }
}

PatternLibraryError PatternLibrary::save_to_file(const std::string& filename) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    try {
        // ✅ Path traversal protection: Validate output path
        std::filesystem::path file_path;
        std::filesystem::path parent_path;

        try {
            file_path = std::filesystem::absolute(filename);
            parent_path = file_path.parent_path();

            // Create parent directory if it doesn't exist
            if (!std::filesystem::exists(parent_path)) {
                std::filesystem::create_directories(parent_path);
            }
        } catch (const std::filesystem::filesystem_error&) {
            return PatternLibraryError::FileAccessDenied;
        }

        // ✅ Restrict to allowed directories
        std::filesystem::path current_dir = std::filesystem::current_path();
        std::filesystem::path allowed_dir = current_dir / "patterns";

        std::string file_str = file_path.string();
        std::string current_str = current_dir.string();
        std::string allowed_str = allowed_dir.string();

        // Allow files in current directory or patterns/ subdirectory
        bool in_current = file_str.find(current_str) == 0;
        bool in_patterns = file_str.find(allowed_str) == 0;

        if (!in_current && !in_patterns) {
            // ✅ Reject path traversal attempts
            return PatternLibraryError::FileAccessDenied;
        }

        // Write file (using validated path)
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return PatternLibraryError::FileAccessDenied;
        }

        file << "# ArgoSentry Pattern Library\n";
        file << "# Format: name|pattern|description|game|version|tags\n\n";

        for (const auto& [name, entry] : patterns_) {
            file << entry.name << "|"
                << entry.pattern << "|"
                << entry.description << "|"
                << entry.game << "|"
                << entry.version << "|";

            for (size_t i = 0; i < entry.tags.size(); ++i) {
                file << entry.tags[i];
                if (i < entry.tags.size() - 1) {
                    file << ",";
                }
            }
            file << "\n";
        }

        return PatternLibraryError::Success;

    }
    catch (const std::exception&) {
        return PatternLibraryError::ParseError;
    }
}

PatternLibraryError PatternLibrary::add_pattern(const PatternEntry& entry) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (!entry.is_valid()) {
        return PatternLibraryError::InvalidPattern;
    }

    if (patterns_.size() >= MAX_PATTERNS &&
        patterns_.find(entry.name) == patterns_.end()) {
        return PatternLibraryError::FileTooLarge;
    }

    patterns_[entry.name] = entry;
    stats_.total_patterns = patterns_.size();

    return PatternLibraryError::Success;
}

void PatternLibrary::remove_pattern(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    patterns_.erase(name);
    stats_.total_patterns = patterns_.size();
}

std::optional<PatternEntry> PatternLibrary::get_pattern(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    stats_.total_searches++;

    auto it = patterns_.find(name);
    if (it != patterns_.end()) {
        stats_.cache_hits++;
        return it->second;
    }

    return std::nullopt;
}

std::vector<PatternEntry> PatternLibrary::search_by_tag(const std::string& tag) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<PatternEntry> results;
    for (const auto& [name, entry] : patterns_) {
        if (std::find(entry.tags.begin(), entry.tags.end(), tag) != entry.tags.end()) {
            results.push_back(entry);
        }
    }

    return results;
}

std::vector<PatternEntry> PatternLibrary::search_by_game(const std::string& game) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<PatternEntry> results;
    for (const auto& [name, entry] : patterns_) {
        if (entry.game == game) {
            results.push_back(entry);
        }
    }

    return results;
}

std::vector<PatternEntry> PatternLibrary::get_all_patterns() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<PatternEntry> results;
    results.reserve(patterns_.size());

    for (const auto& [name, entry] : patterns_) {
        results.push_back(entry);
    }

    return results;
}

void PatternLibrary::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    patterns_.clear();
    stats_ = Stats{};
}

size_t PatternLibrary::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return patterns_.size();
}

bool PatternLibrary::empty() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return patterns_.empty();
}

const PatternLibrary::Stats& PatternLibrary::get_stats() const {
    return stats_;
}

}  // namespace ArgoSentry
