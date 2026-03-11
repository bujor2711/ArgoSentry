#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace ArgoSentry {
namespace Config {

/**
 * @brief DMA Configuration Manager
 * 
 * Manages runtime configuration for VolkDMA library.
 * Supports loading from INI files and runtime modification.
 * 
 * Example config.ini:
 * [FPGA]
 * algorithm = 0
 * min_version_major = 4
 * min_version_minor = 7
 * 
 * [Scanning]
 * chunk_size = 1048576  ; 1MB
 * 
 * [Memory]
 * max_safe_read_size = 2147483648  ; 2GB
 * 
 * [Metrics]
 * enabled = true
 * 
 * [Logging]
 * level = info  ; debug, info, warn, error
 */
class DMAConfiguration {
public:
    // Default configuration values
    struct DefaultConfig {
        // FPGA Settings
        static constexpr int FPGA_ALGORITHM = 0;
        static constexpr uint64_t FPGA_MIN_VERSION_MAJOR = 4;
        static constexpr uint64_t FPGA_MIN_VERSION_MAJOR_ALT = 5;
        static constexpr uint64_t FPGA_MIN_VERSION_MINOR = 7;
        
        // Scanning Settings
        static constexpr size_t SCAN_CHUNK_SIZE = 1024 * 1024; // 1MB
        
        // Memory Settings
        static constexpr size_t MAX_SAFE_READ_SIZE = 2ULL * 1024 * 1024 * 1024; // 2GB
        
        // Metrics Settings
        static constexpr bool METRICS_ENABLED = true;
        
        // Logging Settings
        static constexpr const char* LOG_LEVEL = "info";
    };

    DMAConfiguration();
    ~DMAConfiguration() = default;

    // Disable copy, enable move
    DMAConfiguration(const DMAConfiguration&) = delete;
    DMAConfiguration& operator=(const DMAConfiguration&) = delete;
    DMAConfiguration(DMAConfiguration&&) = default;
    DMAConfiguration& operator=(DMAConfiguration&&) = default;

    /**
     * @brief Load configuration from INI file
     * @param filepath Path to config file
     * @return true if loaded successfully, false otherwise
     */
    bool load_from_file(const std::string& filepath);

    /**
     * @brief Save current configuration to INI file
     * @param filepath Path to save config file
     * @return true if saved successfully, false otherwise
     */
    bool save_to_file(const std::string& filepath) const;

    /**
     * @brief Reset all settings to defaults
     */
    void reset_to_defaults();

    // FPGA Settings Accessors
    [[nodiscard]] int get_fpga_algorithm() const { return fpga_algorithm_; }
    void set_fpga_algorithm(int value) { fpga_algorithm_ = value; }

    [[nodiscard]] uint64_t get_fpga_min_version_major() const { return fpga_min_version_major_; }
    void set_fpga_min_version_major(uint64_t value) { fpga_min_version_major_ = value; }

    [[nodiscard]] uint64_t get_fpga_min_version_major_alt() const { return fpga_min_version_major_alt_; }
    void set_fpga_min_version_major_alt(uint64_t value) { fpga_min_version_major_alt_ = value; }

    [[nodiscard]] uint64_t get_fpga_min_version_minor() const { return fpga_min_version_minor_; }
    void set_fpga_min_version_minor(uint64_t value) { fpga_min_version_minor_ = value; }

    // Scanning Settings Accessors
    [[nodiscard]] size_t get_scan_chunk_size() const { return scan_chunk_size_; }
    void set_scan_chunk_size(size_t value) { scan_chunk_size_ = value; }

    // Memory Settings Accessors
    [[nodiscard]] size_t get_max_safe_read_size() const { return max_safe_read_size_; }
    void set_max_safe_read_size(size_t value) { max_safe_read_size_ = value; }

    // Metrics Settings Accessors
    [[nodiscard]] bool is_metrics_enabled() const { return metrics_enabled_; }
    void set_metrics_enabled(bool value) { metrics_enabled_ = value; }

    // Logging Settings Accessors
    [[nodiscard]] const std::string& get_log_level() const { return log_level_; }
    void set_log_level(const std::string& level) { log_level_ = level; }

    /**
     * @brief Get singleton instance
     * @return Reference to global configuration instance
     */
    static DMAConfiguration& get_instance();

private:
    // FPGA Settings
    int fpga_algorithm_;
    uint64_t fpga_min_version_major_;
    uint64_t fpga_min_version_major_alt_;
    uint64_t fpga_min_version_minor_;

    // Scanning Settings
    size_t scan_chunk_size_;

    // Memory Settings
    size_t max_safe_read_size_;

    // Metrics Settings
    bool metrics_enabled_;

    // Logging Settings
    std::string log_level_;

    // INI parsing helpers
    bool parse_ini_file(const std::string& filepath);
    void parse_ini_line(const std::string& section, const std::string& key, const std::string& value);
    
    // Type conversion helpers
    static std::optional<int> parse_int(const std::string& str);
    static std::optional<uint64_t> parse_uint64(const std::string& str);
    static std::optional<bool> parse_bool(const std::string& str);
    static std::string trim(const std::string& str);
};

// Global accessor for convenience
inline DMAConfiguration& GlobalConfig() {
    return DMAConfiguration::get_instance();
}

} // namespace Config
} // namespace ArgoSentry

