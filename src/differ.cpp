#include "../include/ArgoSentry/differ.hh"
#include "../include/ArgoSentry/dma.hh"
#include <fstream>
#include <algorithm>
#include <thread>
#include <cstring>

namespace ArgoSentry {

MemoryDiffer::MemoryDiffer(const DiffConfig& config)
    : config_(config)
{
    reset_statistics();
}

void MemoryDiffer::reset_statistics() {
    stats_ = DiffStatistics();
}

std::vector<MemoryDiff> MemoryDiffer::compare_snapshots(
    const std::string& snapshot1_path,
    const std::string& snapshot2_path)
{
    auto start_time = std::chrono::steady_clock::now();

    // Read first snapshot
    std::ifstream file1(snapshot1_path, std::ios::binary | std::ios::ate);
    if (!file1.is_open()) {
        return {};
    }

    size_t size1 = static_cast<size_t>(file1.tellg());
    file1.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer1(size1);
    file1.read(reinterpret_cast<char*>(buffer1.data()), size1);
    file1.close();

    // Read second snapshot
    std::ifstream file2(snapshot2_path, std::ios::binary | std::ios::ate);
    if (!file2.is_open()) {
        return {};
    }

    size_t size2 = static_cast<size_t>(file2.tellg());
    file2.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer2(size2);
    file2.read(reinterpret_cast<char*>(buffer2.data()), size2);
    file2.close();

    // Compare buffers
    auto result = compare_buffers(buffer1.data(), size1, buffer2.data(), size2, 0);

    auto end_time = std::chrono::steady_clock::now();
    stats_.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return result;
}

std::vector<MemoryDiff> MemoryDiffer::compare_buffers(
    const uint8_t* buffer1, size_t size1,
    const uint8_t* buffer2, size_t size2,
    uint64_t base_address)
{
    auto start_time = std::chrono::steady_clock::now();

    size_t min_size = (std::min)(size1, size2);
    stats_.total_bytes_compared = min_size;

    auto result = diff_buffers_impl(buffer1, min_size, buffer2, min_size, base_address);

    auto end_time = std::chrono::steady_clock::now();
    stats_.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    stats_.calculate_percentage();

    return result;
}

std::vector<uint64_t> MemoryDiffer::find_changed_addresses(
    DMA& dma,
    uint64_t start_address,
    uint64_t end_address,
    DWORD process_id,
    std::chrono::milliseconds interval)
{
    auto start_time = std::chrono::steady_clock::now();

    size_t size = static_cast<size_t>(end_address - start_address);
    if (size == 0 || size > 100 * 1024 * 1024) { // Max 100MB
        return {};
    }

    // First read using batch_read_range
    std::vector<uint8_t> buffer1;
    try {
        buffer1 = dma.batch_read_range(start_address, end_address, 1024 * 1024, process_id);
        if (buffer1.empty()) {
            return {};
        }
    } catch (...) {
        return {};
    }

    // Wait interval
    std::this_thread::sleep_for(interval);

    // Second read
    std::vector<uint8_t> buffer2;
    try {
        buffer2 = dma.batch_read_range(start_address, end_address, 1024 * 1024, process_id);
        if (buffer2.empty()) {
            return {};
        }
    } catch (...) {
        return {};
    }

    // Find differences
    std::vector<uint64_t> changed_addresses;
    stats_.total_bytes_compared = (std::min)(buffer1.size(), buffer2.size());
    size_t compare_size = stats_.total_bytes_compared;

    for (size_t i = 0; i < compare_size; ++i) {
        if (buffer1[i] != buffer2[i]) {
            changed_addresses.push_back(start_address + i);
            stats_.bytes_changed++;

            if (changed_addresses.size() >= config_.max_results) {
                break;
            }
        }
    }

    stats_.total_changes_found = changed_addresses.size();

    auto end_time = std::chrono::steady_clock::now();
    stats_.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    stats_.calculate_percentage();

    return changed_addresses;
}

std::vector<MemoryDiff> MemoryDiffer::compare_regions(
    DMA& dma,
    uint64_t start_address,
    uint64_t end_address,
    DWORD process_id,
    std::chrono::milliseconds interval)
{
    auto start_time = std::chrono::steady_clock::now();

    size_t size = static_cast<size_t>(end_address - start_address);
    if (size == 0 || size > 100 * 1024 * 1024) { // Max 100MB
        return {};
    }

    // First read
    std::vector<uint8_t> buffer1;
    try {
        buffer1 = dma.batch_read_range(start_address, end_address, 1024 * 1024, process_id);
        if (buffer1.empty()) {
            return {};
        }
    } catch (...) {
        return {};
    }

    // Wait interval
    std::this_thread::sleep_for(interval);

    // Second read
    std::vector<uint8_t> buffer2;
    try {
        buffer2 = dma.batch_read_range(start_address, end_address, 1024 * 1024, process_id);
        if (buffer2.empty()) {
            return {};
        }
    } catch (...) {
        return {};
    }

    // Compare buffers
    size_t compare_size = (std::min)(buffer1.size(), buffer2.size());
    auto result = compare_buffers(buffer1.data(), compare_size, buffer2.data(), compare_size, start_address);

    auto end_time = std::chrono::steady_clock::now();
    stats_.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return result;
}

std::vector<uint64_t> MemoryDiffer::find_value(
    DMA& dma,
    uint64_t start_address,
    uint64_t end_address,
    DWORD process_id,
    const std::vector<uint8_t>& value)
{
    if (value.empty()) {
        return {};
    }

    auto start_time = std::chrono::steady_clock::now();

    size_t region_size = static_cast<size_t>(end_address - start_address);
    if (region_size == 0 || region_size > 100 * 1024 * 1024) { // Max 100MB
        return {};
    }

    // Read memory region
    std::vector<uint8_t> buffer;
    try {
        buffer = dma.batch_read_range(start_address, end_address, 1024 * 1024, process_id);
        if (buffer.empty()) {
            return {};
        }
    } catch (...) {
        return {};
    }

    // Search for value
    std::vector<uint64_t> found_addresses;
    size_t value_size = value.size();
    size_t search_size = buffer.size();

    if (value_size > search_size) {
        return {};
    }

    for (size_t i = 0; i <= search_size - value_size; ++i) {
        if (std::memcmp(&buffer[i], value.data(), value_size) == 0) {
            found_addresses.push_back(start_address + i);

            if (found_addresses.size() >= config_.max_results) {
                break;
            }
        }
    }

    stats_.total_bytes_compared = search_size;
    stats_.total_changes_found = found_addresses.size();

    auto end_time = std::chrono::steady_clock::now();
    stats_.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return found_addresses;
}

std::vector<uint64_t> MemoryDiffer::filter_changed(
    DMA& dma,
    const std::vector<uint64_t>& addresses,
    DWORD process_id,
    std::chrono::milliseconds interval)
{
    if (addresses.empty()) {
        return {};
    }

    auto start_time = std::chrono::steady_clock::now();

    // Read first snapshot of all addresses
    std::vector<std::vector<uint8_t>> snapshots1;
    snapshots1.reserve(addresses.size());

    for (uint64_t addr : addresses) {
        std::vector<uint8_t> buffer;
        try {
            // Read config_.max_change_size bytes starting at this address
            auto data = dma.batch_read_range(addr, addr + config_.max_change_size, config_.max_change_size, process_id);
            if (!data.empty()) {
                buffer = std::move(data);
            }
            snapshots1.push_back(std::move(buffer));
        } catch (...) {
            snapshots1.push_back({});
        }
    }

    // Wait interval
    std::this_thread::sleep_for(interval);

    // Read second snapshot and compare
    std::vector<uint64_t> changed_addresses;

    for (size_t i = 0; i < addresses.size(); ++i) {
        if (snapshots1[i].empty()) {
            continue;
        }

        std::vector<uint8_t> buffer2;
        try {
            buffer2 = dma.batch_read_range(addresses[i], addresses[i] + config_.max_change_size, config_.max_change_size, process_id);
            if (buffer2.empty()) {
                continue;
            }
        } catch (...) {
            continue;
        }

        // Check if changed
        size_t compare_size = (std::min)(snapshots1[i].size(), buffer2.size());
        if (std::memcmp(snapshots1[i].data(), buffer2.data(), compare_size) != 0) {
            changed_addresses.push_back(addresses[i]);
        }
    }

    stats_.total_bytes_compared = addresses.size() * config_.max_change_size;
    stats_.total_changes_found = changed_addresses.size();

    auto end_time = std::chrono::steady_clock::now();
    stats_.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return changed_addresses;
}

std::vector<MemoryDiff> MemoryDiffer::diff_buffers_impl(
    const uint8_t* buf1, size_t size1,
    const uint8_t* buf2, size_t size2,
    uint64_t base_addr)
{
    std::vector<MemoryDiff> diffs;
    size_t min_size = (std::min)(size1, size2);

    size_t i = 0;
    while (i < min_size) {
        // Find start of difference
        while (i < min_size && buf1[i] == buf2[i]) {
            ++i;
        }

        if (i >= min_size) {
            break;
        }

        // Found difference, determine extent
        size_t diff_start = i;
        size_t diff_size = 0;

        while (i < min_size && diff_size < config_.max_change_size) {
            if (buf1[i] != buf2[i]) {
                diff_size = (i - diff_start) + 1;
                ++i;
            } else if (config_.group_adjacent) {
                // Check if we should group with next change
                size_t gap = 0;
                size_t j = i;
                while (j < min_size && gap < config_.adjacency_threshold && buf1[j] == buf2[j]) {
                    ++gap;
                    ++j;
                }

                if (j < min_size && buf1[j] != buf2[j]) {
                    // Continue grouping
                    i = j;
                    continue;
                } else {
                    // End of difference
                    break;
                }
            } else {
                // Don't group, end of difference
                break;
            }
        }

        // Apply size constraints
        if (diff_size >= config_.min_change_size && diff_size <= config_.max_change_size) {
            std::vector<uint8_t> before(buf1 + diff_start, buf1 + diff_start + diff_size);
            std::vector<uint8_t> after(buf2 + diff_start, buf2 + diff_start + diff_size);

            diffs.emplace_back(base_addr + diff_start, before, after);
            stats_.bytes_changed += diff_size;

            if (diffs.size() >= config_.max_results) {
                break;
            }
        }
    }

    stats_.total_changes_found = diffs.size();
    return diffs;
}

bool MemoryDiffer::should_group_with_previous(const MemoryDiff& prev, uint64_t current_addr) const {
    if (!config_.group_adjacent) {
        return false;
    }

    uint64_t prev_end = prev.address + prev.size;
    uint64_t gap = current_addr - prev_end;

    return gap <= config_.adjacency_threshold;
}

} // namespace ArgoSentry


