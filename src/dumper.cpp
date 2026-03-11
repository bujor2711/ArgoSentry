// src/dumper.cpp
// v1.9 Memory Dump Utilities Implementation
// Copyright (c) 2026 VolkDMA Project

#include "ArgoSentry/dumper.hh"
#include "ArgoSentry/dma.hh"
#include "ArgoSentry/memory_layout.hh"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <cstring>

namespace ArgoSentry {

// ============================================================================
// Constructor & Destructor
// ============================================================================

MemoryDumper::MemoryDumper(DMA& dma)
    : dma_(dma)
    , chunk_size_(1024 * 1024)  // 1MB default
    , compression_enabled_(false)
    , auto_metadata_(true)
{
}

MemoryDumper::~MemoryDumper() = default;

// ============================================================================
// Core Dump Methods
// ============================================================================

bool MemoryDumper::dump_region(uint64_t start_address, uint64_t end_address,
                                const std::string& filename, DWORD process_id,
                                DumpFormat format)
{
    // Validate parameters
    if (end_address <= start_address) {
        return false;
    }

    const uint64_t total_size = end_address - start_address;
    uint64_t current_address = start_address;
    size_t bytes_written = 0;

    // Open output file
    std::ofstream outfile;
    if (format == DumpFormat::Binary || format == DumpFormat::IDA) {
        outfile.open(filename, std::ios::binary);
    } else {
        outfile.open(filename);
    }

    if (!outfile.is_open()) {
        return false;
    }

    // Write header for text formats
    if (format == DumpFormat::HexDump) {
        outfile << "Memory Dump: 0x" << std::hex << std::uppercase 
                << start_address << " - 0x" << end_address << std::dec << "\n";
        outfile << "Size: " << total_size << " bytes\n";
        outfile << "Process ID: " << process_id << "\n";
        outfile << "Timestamp: " << generate_timestamp() << "\n";
        outfile << "----------------------------------------\n";
    } else if (format == DumpFormat::CArray) {
        outfile << "// Memory dump from 0x" << std::hex << std::uppercase 
                << start_address << " - 0x" << end_address << std::dec << "\n";
        outfile << "// Size: " << total_size << " bytes\n";
        outfile << "// Process ID: " << process_id << "\n";
        outfile << "// Timestamp: " << generate_timestamp() << "\n\n";
    }

    // Read and write in chunks
    std::vector<uint8_t> buffer(chunk_size_);
    bool had_errors = false;

    while (current_address < end_address) {
        const size_t remaining = static_cast<size_t>(end_address - current_address);
        const size_t read_size = std::min(chunk_size_, remaining);

        try {
            // Read chunk from target process
            const size_t bytes_read = dma_.read_bytes(current_address, buffer.data(), 
                                                       read_size, process_id);

            if (bytes_read > 0) {
                // Write chunk in requested format
                switch (format) {
                    case DumpFormat::Binary:
                        write_binary_dump(outfile, buffer.data(), bytes_read);
                        break;
                    case DumpFormat::HexDump:
                        write_hex_dump(outfile, buffer.data(), bytes_read, current_address);
                        break;
                    case DumpFormat::CArray:
                        write_carray_dump(outfile, buffer.data(), bytes_read, 
                                         bytes_written == 0, 
                                         current_address + bytes_read >= end_address);
                        break;
                    case DumpFormat::IDA:
                        write_binary_dump(outfile, buffer.data(), bytes_read);
                        break;
                }

                bytes_written += bytes_read;
                current_address += bytes_read;
            } else {
                // Skip failed region
                current_address += read_size;
                had_errors = true;
            }
        } catch (...) {
            // Skip failed region
            current_address += read_size;
            had_errors = true;
        }
    }

    // Close array for CArray format
    if (format == DumpFormat::CArray && bytes_written > 0) {
        outfile << "\n};\n";
        outfile << "const size_t dump_size = " << bytes_written << ";\n";
    }

    outfile.close();

    // Create IDA script if IDA format
    if (format == DumpFormat::IDA && bytes_written > 0) {
        write_ida_script(filename, start_address, bytes_written);
    }

    // Save metadata if enabled
    if (auto_metadata_ && bytes_written > 0) {
        DumpMetadata metadata;
        metadata.base_address = start_address;
        metadata.size = bytes_written;
        metadata.process_id = process_id;
        metadata.timestamp = generate_timestamp();
        metadata.format = format;
        save_metadata(filename + ".meta", metadata);
    }

    return bytes_written > 0 && !had_errors;
}

bool MemoryDumper::dump_module(const std::string& module_name, 
                                const std::string& filename,
                                DWORD process_id, DumpFormat format)
{
    // Use memory layout analyzer to find module
    MemoryLayoutAnalyzer analyzer(dma_);
    
    auto module_region = analyzer.find_module(module_name, process_id);
    if (!module_region) {
        return false;
    }

    // Dump the module region
    const uint64_t end_address = module_region->base_address + module_region->size;
    bool success = dump_region(module_region->base_address, end_address, 
                                filename, process_id, format);

    // Update metadata with module name
    if (success && auto_metadata_) {
        auto metadata = load_metadata(filename + ".meta");
        if (metadata) {
            metadata->module_name = module_name;
            save_metadata(filename + ".meta", *metadata);
        }
    }

    return success;
}

bool MemoryDumper::create_snapshot(DWORD process_id, const std::string& snapshot_dir)
{
    // Create snapshot directory
    std::filesystem::path dir_path(snapshot_dir);
    try {
        std::filesystem::create_directories(dir_path);
    } catch (...) {
        return false;
    }

    // Get memory layout
    MemoryLayoutAnalyzer analyzer(dma_);
    auto regions = analyzer.get_memory_layout(process_id);

    if (regions.empty()) {
        return false;
    }

    // Create manifest file
    std::ofstream manifest(dir_path / "manifest.txt");
    if (!manifest.is_open()) {
        return false;
    }

    manifest << "Memory Snapshot\n";
    manifest << "Process ID: " << process_id << "\n";
    manifest << "Timestamp: " << generate_timestamp() << "\n";
    manifest << "Total Regions: " << regions.size() << "\n";
    manifest << "----------------------------------------\n\n";

    // Dump each committed region
    size_t region_count = 0;
    size_t total_bytes = 0;

    for (const auto& region : regions) {
        if (!region.is_committed()) {
            continue;
        }

        // Generate filename
        std::stringstream ss;
        ss << "region_" << std::setw(4) << std::setfill('0') << region_count
           << "_0x" << std::hex << std::uppercase << region.base_address << ".bin";

        const std::string region_file = ss.str();
        const std::filesystem::path region_path = dir_path / region_file;

        // Dump region
        const uint64_t end_address = region.base_address + region.size;
        if (dump_region(region.base_address, end_address, 
                        region_path.string(), process_id, DumpFormat::Binary)) {
            // Write to manifest
            manifest << "Region " << region_count << ":\n";
            manifest << "  File: " << region_file << "\n";
            manifest << "  Base: 0x" << std::hex << region.base_address << std::dec << "\n";
            manifest << "  Size: " << region.size << " bytes\n";
            manifest << "  Type: ";
            
            if (region.is_image()) manifest << "IMAGE ";
            if (region.is_readable()) manifest << "R";
            if (region.is_writable()) manifest << "W";
            if (region.is_executable()) manifest << "X";
            
            manifest << "\n";
            if (!region.module_name.empty()) {
                manifest << "  Module: " << region.module_name << "\n";
            }
            manifest << "\n";

            region_count++;
            total_bytes += region.size;
        }
    }

    manifest << "----------------------------------------\n";
    manifest << "Total Dumped: " << region_count << " regions\n";
    manifest << "Total Size: " << total_bytes << " bytes\n";
    manifest.close();

    return region_count > 0;
}

void MemoryDumper::print_hex_dump(uint64_t address, size_t size, DWORD process_id)
{
    std::vector<uint8_t> buffer(size);
    
    try {
        const size_t bytes_read = dma_.read_bytes(address, buffer.data(), size, process_id);

        if (bytes_read == 0) {
            std::cout << "Failed to read memory at 0x" << std::hex << std::uppercase 
                      << address << std::dec << "\n";
            return;
        }

        // Print header
        std::cout << "\nMemory Dump at 0x" << std::hex << std::uppercase << address 
                  << std::dec << " (" << bytes_read << " bytes):\n";
        std::cout << "----------------------------------------\n";

        // Print hex dump
        for (size_t i = 0; i < bytes_read; i += 16) {
            const size_t line_size = std::min(size_t(16), bytes_read - i);
            std::cout << format_hex_line(buffer.data() + i, line_size, address + i) << "\n";
        }

        std::cout << "----------------------------------------\n";
    } catch (...) {
        std::cout << "Exception while reading memory\n";
    }
}

// ============================================================================
// Dump Comparison Methods
// ============================================================================

std::vector<uint64_t> MemoryDumper::compare_dumps(const std::string& file1,
                                                   const std::string& file2)
{
    std::vector<uint64_t> changed_addresses;

    // Open both files
    std::ifstream f1(file1, std::ios::binary);
    std::ifstream f2(file2, std::ios::binary);

    if (!f1.is_open() || !f2.is_open()) {
        return changed_addresses;
    }

    // Load metadata to get base addresses
    auto meta1 = load_metadata(file1 + ".meta");
    auto meta2 = load_metadata(file2 + ".meta");

    const uint64_t base_addr = meta1 ? meta1->base_address : 0;

    // Compare byte by byte
    const size_t buffer_size = 4096;
    std::vector<uint8_t> buf1(buffer_size);
    std::vector<uint8_t> buf2(buffer_size);
    uint64_t offset = 0;

    while (true) {
        f1.read(reinterpret_cast<char*>(buf1.data()), buffer_size);
        f2.read(reinterpret_cast<char*>(buf2.data()), buffer_size);

        const std::streamsize read1 = f1.gcount();
        const std::streamsize read2 = f2.gcount();

        if (read1 == 0 && read2 == 0) {
            break; // Both files exhausted
        }

        const size_t compare_size = std::min(read1, read2);

        for (size_t i = 0; i < compare_size; ++i) {
            if (buf1[i] != buf2[i]) {
                changed_addresses.push_back(base_addr + offset + i);
            }
        }

        offset += compare_size;

        if (read1 < buffer_size && read2 < buffer_size) {
            break;
        }
    }

    return changed_addresses;
}

std::vector<MemoryDumper::MemoryDiff> MemoryDumper::get_detailed_diff(
    const std::string& file1, const std::string& file2, size_t context_bytes)
{
    std::vector<MemoryDiff> differences;

    // Get changed addresses
    auto changed = compare_dumps(file1, file2);
    if (changed.empty()) {
        return differences;
    }

    // Load metadata
    auto meta1 = load_metadata(file1 + ".meta");
    const uint64_t base_addr = meta1 ? meta1->base_address : 0;

    // Open files
    std::ifstream f1(file1, std::ios::binary);
    std::ifstream f2(file2, std::ios::binary);

    if (!f1.is_open() || !f2.is_open()) {
        return differences;
    }

    // Read both files into memory
    std::vector<uint8_t> data1((std::istreambuf_iterator<char>(f1)),
                                std::istreambuf_iterator<char>());
    std::vector<uint8_t> data2((std::istreambuf_iterator<char>(f2)),
                                std::istreambuf_iterator<char>());

    // Build detailed diff with context
    for (uint64_t addr : changed) {
        const uint64_t offset = addr - base_addr;

        if (offset >= data1.size() || offset >= data2.size()) {
            continue;
        }

        MemoryDiff diff;
        diff.address = addr;

        // Calculate context range
        const uint64_t start_offset = (offset >= context_bytes) ? (offset - context_bytes) : 0;
        const uint64_t end_offset = std::min(offset + context_bytes + 1,
                                             std::min(data1.size(), data2.size()));

        // Extract before/after with context
        diff.before.assign(data1.begin() + start_offset, data1.begin() + end_offset);
        diff.after.assign(data2.begin() + start_offset, data2.begin() + end_offset);
        diff.size = static_cast<size_t>(end_offset - start_offset);

        differences.push_back(diff);
    }

    return differences;
}

// ============================================================================
// Metadata Methods
// ============================================================================

bool MemoryDumper::save_metadata(const std::string& filename, 
                                  const DumpMetadata& metadata)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "{\n";
    file << "  \"base_address\": \"0x" << std::hex << std::uppercase 
         << metadata.base_address << std::dec << "\",\n";
    file << "  \"size\": " << metadata.size << ",\n";
    file << "  \"process_name\": \"" << metadata.process_name << "\",\n";
    file << "  \"process_id\": " << metadata.process_id << ",\n";
    file << "  \"timestamp\": \"" << metadata.timestamp << "\",\n";
    file << "  \"format\": \"" << format_to_string(metadata.format) << "\"";
    
    if (!metadata.module_name.empty()) {
        file << ",\n  \"module_name\": \"" << metadata.module_name << "\"";
    }
    
    file << "\n}\n";
    file.close();

    return true;
}

std::optional<DumpMetadata> MemoryDumper::load_metadata(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return std::nullopt;
    }

    DumpMetadata metadata{};
    std::string line;

    while (std::getline(file, line)) {
        // Simple JSON parsing (enough for our format)
        if (line.find("\"base_address\"") != std::string::npos) {
            const size_t start = line.find("0x") + 2;
            const size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                metadata.base_address = std::stoull(line.substr(start, end - start), nullptr, 16);
            }
        } else if (line.find("\"size\"") != std::string::npos) {
            const size_t start = line.find(":") + 1;
            const size_t end = line.find(",", start);
            if (start != std::string::npos) {
                metadata.size = std::stoull(line.substr(start, end - start));
            }
        } else if (line.find("\"process_id\"") != std::string::npos) {
            const size_t start = line.find(":") + 1;
            const size_t end = line.find(",", start);
            if (start != std::string::npos) {
                metadata.process_id = std::stoul(line.substr(start, end - start));
            }
        } else if (line.find("\"format\"") != std::string::npos) {
            const size_t start = line.find("\"", line.find(":")) + 1;
            const size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                metadata.format = string_to_format(line.substr(start, end - start));
            }
        }
    }

    file.close();
    return metadata;
}

// ============================================================================
// Configuration Methods
// ============================================================================

void MemoryDumper::set_chunk_size(size_t size) {
    if (size > 0) {
        chunk_size_ = size;
    }
}

size_t MemoryDumper::get_chunk_size() const {
    return chunk_size_;
}

void MemoryDumper::set_compression_enabled(bool enabled) {
    compression_enabled_ = enabled;
}

bool MemoryDumper::is_compression_enabled() const {
    return compression_enabled_;
}

void MemoryDumper::set_auto_metadata_enabled(bool enabled) {
    auto_metadata_ = enabled;
}

bool MemoryDumper::is_auto_metadata_enabled() const {
    return auto_metadata_;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

std::string MemoryDumper::format_hex_line(const uint8_t* data, size_t size, 
                                          uint64_t address) const
{
    std::stringstream ss;

    // Address
    ss << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') 
       << address << ": ";

    // Hex bytes
    for (size_t i = 0; i < 16; ++i) {
        if (i < size) {
            ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
               << static_cast<int>(data[i]) << " ";
        } else {
            ss << "   ";
        }

        if (i == 7) {
            ss << " ";
        }
    }

    ss << " | ";

    // ASCII representation
    for (size_t i = 0; i < size; ++i) {
        const char c = static_cast<char>(data[i]);
        ss << (std::isprint(c) ? c : '.');
    }

    return ss.str();
}

std::string MemoryDumper::format_carray(const uint8_t* data, size_t size, 
                                        bool is_first_chunk) const
{
    std::stringstream ss;

    if (is_first_chunk) {
        ss << "const unsigned char memory_dump[] = {\n    ";
    }

    for (size_t i = 0; i < size; ++i) {
        ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
           << static_cast<int>(data[i]);

        if (i < size - 1) {
            ss << ", ";
        }

        if ((i + 1) % 12 == 0) {
            ss << "\n    ";
        }
    }

    return ss.str();
}

bool MemoryDumper::write_ida_script(const std::string& binary_file, 
                                     uint64_t base_address, size_t size)
{
    const std::string script_file = binary_file + ".idc";
    std::ofstream script(script_file);

    if (!script.is_open()) {
        return false;
    }

    script << "// IDA Pro Import Script\n";
    script << "// Generated: " << generate_timestamp() << "\n\n";
    script << "#include <idc.idc>\n\n";
    script << "static main() {\n";
    script << "    auto file = \"" << binary_file << "\";\n";
    script << "    auto base = 0x" << std::hex << std::uppercase << base_address << ";\n";
    script << "    auto size = 0x" << size << ";\n\n";
    script << "    // Load binary at base address\n";
    script << "    loadfile(file, 0, base, size);\n\n";
    script << "    Message(\"Loaded memory dump at 0x%X (%d bytes)\\n\", base, size);\n";
    script << "}\n";

    script.close();
    return true;
}

bool MemoryDumper::write_binary_dump(const std::string& filename, 
                                      const std::vector<uint8_t>& data)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    return true;
}

bool MemoryDumper::write_hex_dump(const std::string& filename,
                                   uint64_t base_address,
                                   const std::vector<uint8_t>& data)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    for (size_t i = 0; i < data.size(); i += 16) {
        const size_t line_size = std::min(size_t(16), data.size() - i);
        file << format_hex_line(base_address + i, data, i, line_size) << "\n";
    }

    file.close();
    return true;
}

bool MemoryDumper::write_carray_dump(const std::string& filename,
                                      uint64_t base_address,
                                      const std::vector<uint8_t>& data)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << format_carray(data, base_address);
    file.close();
    return true;
                                      size_t size, bool is_first, bool is_last) const
{
    if (is_first) {
        file << "const unsigned char memory_dump[] = {\n    ";
    }

    for (size_t i = 0; i < size; ++i) {
        file << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
             << static_cast<int>(data[i]);

        if (!is_last || i < size - 1) {
            file << ", ";
        }

        if ((i + 1) % 12 == 0 && i < size - 1) {
            file << "\n    ";
        }
    }

    if (!is_last) {
        file << "\n    ";
    }
}

std::string MemoryDumper::generate_timestamp() const
{
    const auto now = std::chrono::system_clock::now();
    const auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string MemoryDumper::format_to_string(DumpFormat format) const
{
    switch (format) {
        case DumpFormat::Binary: return "Binary";
        case DumpFormat::HexDump: return "HexDump";
        case DumpFormat::CArray: return "CArray";
        case DumpFormat::IDA: return "IDA";
        default: return "Unknown";
    }
}

DumpFormat MemoryDumper::string_to_format(const std::string& str) const
{
    if (str == "HexDump") return DumpFormat::HexDump;
    if (str == "CArray") return DumpFormat::CArray;
    if (str == "IDA") return DumpFormat::IDA;
    return DumpFormat::Binary;
}

} // namespace ArgoSentry


