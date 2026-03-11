#include "include/VolkDMA/config.hh"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace VolkDMA {
namespace Config {

DMAConfiguration::DMAConfiguration() {
    reset_to_defaults();
}

void DMAConfiguration::reset_to_defaults() {
    fpga_algorithm_ = DefaultConfig::FPGA_ALGORITHM;
    fpga_min_version_major_ = DefaultConfig::FPGA_MIN_VERSION_MAJOR;
    fpga_min_version_major_alt_ = DefaultConfig::FPGA_MIN_VERSION_MAJOR_ALT;
    fpga_min_version_minor_ = DefaultConfig::FPGA_MIN_VERSION_MINOR;
    scan_chunk_size_ = DefaultConfig::SCAN_CHUNK_SIZE;
    max_safe_read_size_ = DefaultConfig::MAX_SAFE_READ_SIZE;
    metrics_enabled_ = DefaultConfig::METRICS_ENABLED;
    log_level_ = DefaultConfig::LOG_LEVEL;
}

bool DMAConfiguration::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to open config file: " << filepath << std::endl;
        return false;
    }

    std::string line;
    std::string current_section;
    int line_number = 0;

    while (std::getline(file, line)) {
        ++line_number;
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            current_section = trim(current_section);
            continue;
        }

        // Parse key=value pair
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            std::cerr << "[Config] Invalid line " << line_number << ": " << line << std::endl;
            continue;
        }

        std::string key = trim(line.substr(0, equals_pos));
        std::string value = line.substr(equals_pos + 1);
        
        // Remove inline comments
        size_t comment_pos = value.find_first_of(";#");
        if (comment_pos != std::string::npos) {
            value = value.substr(0, comment_pos);
        }
        value = trim(value);

        parse_ini_line(current_section, key, value);
    }

    std::cout << "[Config] Successfully loaded configuration from: " << filepath << std::endl;
    return true;
}

bool DMAConfiguration::save_to_file(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to create config file: " << filepath << std::endl;
        return false;
    }

    file << "; VolkDMA Configuration File\n";
    file << "; Auto-generated\n\n";

    // FPGA Section
    file << "[FPGA]\n";
    file << "algorithm = " << fpga_algorithm_ << "\n";
    file << "min_version_major = " << fpga_min_version_major_ << "\n";
    file << "min_version_major_alt = " << fpga_min_version_major_alt_ << "\n";
    file << "min_version_minor = " << fpga_min_version_minor_ << "\n\n";

    // Scanning Section
    file << "[Scanning]\n";
    file << "chunk_size = " << scan_chunk_size_ << "  ; bytes\n\n";

    // Memory Section
    file << "[Memory]\n";
    file << "max_safe_read_size = " << max_safe_read_size_ << "  ; bytes\n\n";

    // Metrics Section
    file << "[Metrics]\n";
    file << "enabled = " << (metrics_enabled_ ? "true" : "false") << "\n\n";

    // Logging Section
    file << "[Logging]\n";
    file << "level = " << log_level_ << "  ; debug, info, warn, error\n";

    std::cout << "[Config] Successfully saved configuration to: " << filepath << std::endl;
    return true;
}

void DMAConfiguration::parse_ini_line(const std::string& section, const std::string& key, const std::string& value) {
    if (section == "FPGA") {
        if (key == "algorithm") {
            if (auto val = parse_int(value)) {
                fpga_algorithm_ = *val;
            }
        } else if (key == "min_version_major") {
            if (auto val = parse_uint64(value)) {
                fpga_min_version_major_ = *val;
            }
        } else if (key == "min_version_major_alt") {
            if (auto val = parse_uint64(value)) {
                fpga_min_version_major_alt_ = *val;
            }
        } else if (key == "min_version_minor") {
            if (auto val = parse_uint64(value)) {
                fpga_min_version_minor_ = *val;
            }
        }
    } else if (section == "Scanning") {
        if (key == "chunk_size") {
            if (auto val = parse_uint64(value)) {
                scan_chunk_size_ = static_cast<size_t>(*val);
            }
        }
    } else if (section == "Memory") {
        if (key == "max_safe_read_size") {
            if (auto val = parse_uint64(value)) {
                max_safe_read_size_ = static_cast<size_t>(*val);
            }
        }
    } else if (section == "Metrics") {
        if (key == "enabled") {
            if (auto val = parse_bool(value)) {
                metrics_enabled_ = *val;
            }
        }
    } else if (section == "Logging") {
        if (key == "level") {
            log_level_ = value;
        }
    }
}

std::optional<int> DMAConfiguration::parse_int(const std::string& str) {
    try {
        return std::stoi(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<uint64_t> DMAConfiguration::parse_uint64(const std::string& str) {
    try {
        return std::stoull(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> DMAConfiguration::parse_bool(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    } else if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    }
    
    return std::nullopt;
}

std::string DMAConfiguration::trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

DMAConfiguration& DMAConfiguration::get_instance() {
    static DMAConfiguration instance;
    return instance;
}

} // namespace Config
} // namespace VolkDMA
