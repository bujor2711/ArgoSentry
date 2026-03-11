// src/dma_dumper_integration.cpp
// v1.9 Memory Dumper Integration into DMA Class
// Copyright (c) 2026 VolkDMA Project

#include "VolkDMA/dma.hh"
#include "VolkDMA/dumper.hh"
#include <stdexcept>

namespace VolkDMA {

// ============================================================================
// Memory Dumper Integration Methods
// ============================================================================

bool DMA::dump_memory_region(uint64_t start_address, uint64_t end_address,
                              const std::string& filename, DWORD process_id,
                              DumpFormat format)
{
    try {
        MemoryDumper dumper(*this);
        return dumper.dump_region(start_address, end_address, filename, 
                                   process_id, format);
    } catch (const std::exception&) {
        return false;
    }
}

bool DMA::dump_module(const std::string& module_name, const std::string& filename,
                       DWORD process_id, DumpFormat format)
{
    try {
        MemoryDumper dumper(*this);
        return dumper.dump_module(module_name, filename, process_id, format);
    } catch (const std::exception&) {
        return false;
    }
}

bool DMA::create_memory_snapshot(DWORD process_id, const std::string& snapshot_dir)
{
    try {
        MemoryDumper dumper(*this);
        return dumper.create_snapshot(process_id, snapshot_dir);
    } catch (const std::exception&) {
        return false;
    }
}

void DMA::print_hex_dump(uint64_t address, size_t size, DWORD process_id)
{
    try {
        MemoryDumper dumper(*this);
        dumper.print_hex_dump(address, size, process_id);
    } catch (const std::exception& e) {
        std::cerr << "Error printing hex dump: " << e.what() << "\n";
    }
}

std::vector<uint64_t> DMA::compare_memory_dumps(const std::string& file1,
                                                  const std::string& file2)
{
    try {
        MemoryDumper dumper(*this);
        return dumper.compare_dumps(file1, file2);
    } catch (const std::exception&) {
        return std::vector<uint64_t>();
    }
}

} // namespace VolkDMA
