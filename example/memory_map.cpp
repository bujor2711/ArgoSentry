// Memory Map Viewer - Shows valid memory regions for a process
// Helps find valid addresses for memory reading

#include <vmmdll.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

void print_memory_map(VMM_HANDLE hVMM, DWORD pid) {
    std::cout << "\n=== Memory Map for PID " << pid << " ===\n\n";
    
    // Get memory map
    PVMMDLL_MAP_VADEX pVadMap = NULL;
    SIZE_T cbVadMap = 0;
    
    if (!VMMDLL_Map_GetVadEx(hVMM, pid, FALSE, &pVadMap, &cbVadMap)) {
        std::cout << "Failed to get memory map!\n";
        return;
    }
    
    std::cout << "Total regions: " << pVadMap->cMap << "\n\n";
    std::cout << std::hex << std::uppercase;
    std::cout << "Address Range                    Size      Type       Protect\n";
    std::cout << "================================================================\n";
    
    for (DWORD i = 0; i < pVadMap->cMap && i < 50; i++) {
        auto& entry = pVadMap->pMap[i];
        
        // Calculate size
        uint64_t size = entry.vaEnd - entry.vaStart + 1;
        
        // Type
        std::string type = "Private";
        if (entry.fImage) type = "Image";
        else if (entry.fFile) type = "Mapped";
        
        // Print
        std::cout << "0x" << std::setw(16) << std::setfill('0') << entry.vaStart 
                  << " - 0x" << std::setw(16) << entry.vaEnd;
        std::cout << "  " << std::setw(8) << std::right << (size / 1024) << "KB";
        std::cout << "  " << std::setw(8) << std::left << type;
        std::cout << "  " << entry.Protection;
        std::cout << "\n";
    }
    
    std::cout << std::dec;
    VMMDLL_MemFree(pVadMap);
}

void print_modules(VMM_HANDLE hVMM, DWORD pid) {
    std::cout << "\n=== Loaded Modules for PID " << pid << " ===\n\n";
    
    PVMMDLL_MAP_MODULE pModuleMap = NULL;
    
    if (!VMMDLL_Map_GetModuleU(hVMM, pid, &pModuleMap, 0)) {
        std::cout << "Failed to get module list!\n";
        return;
    }
    
    std::cout << std::hex << std::uppercase;
    std::cout << "Base Address          Size      Name\n";
    std::cout << "================================================\n";
    
    for (DWORD i = 0; i < pModuleMap->cMap && i < 20; i++) {
        auto& mod = pModuleMap->pMap[i];
        std::cout << "0x" << std::setw(16) << std::setfill('0') << mod.vaBase;
        std::cout << "  " << std::setw(8) << std::right << (mod.cbImageSize / 1024) << "KB";
        std::cout << "  " << mod.uszText << "\n";
    }
    
    std::cout << std::dec;
    VMMDLL_MemFree(pModuleMap);
}

int main() {
    std::cout << "=== Memory Map Viewer ===\n\n";
    
    // Initialize VMM
    LPCSTR args[] = { "", "-device", "fpga" };
    VMM_HANDLE hVMM = VMMDLL_Initialize(3, (LPSTR*)args);
    
    if (!hVMM) {
        std::cout << "Failed to initialize DMA!\n";
        std::cout << "Make sure to run as Administrator.\n";
        return 1;
    }
    
    std::cout << "DMA initialized successfully!\n";
    
    // Get process name
    std::string process_name;
    std::cout << "Enter process name (e.g., notepad.exe): ";
    std::getline(std::cin, process_name);
    
    // Find process
    SIZE_T cPIDs = 0;
    VMMDLL_PidList(hVMM, NULL, &cPIDs);
    
    DWORD* pids = new DWORD[cPIDs];
    VMMDLL_PidList(hVMM, pids, &cPIDs);
    
    DWORD target_pid = 0;
    for (SIZE_T i = 0; i < cPIDs; i++) {
        VMMDLL_PROCESS_INFORMATION info = {0};
        info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
        info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;
        SIZE_T cbInfo = sizeof(info);
        
        if (VMMDLL_ProcessGetInformation(hVMM, pids[i], &info, &cbInfo)) {
            if (_stricmp(info.szName, process_name.c_str()) == 0) {
                target_pid = pids[i];
                break;
            }
        }
    }
    
    delete[] pids;
    
    if (target_pid == 0) {
        std::cout << "Process not found!\n";
        VMMDLL_Close(hVMM);
        return 1;
    }
    
    std::cout << "Found process PID: " << target_pid << "\n";
    
    // Show modules
    print_modules(hVMM, target_pid);
    
    // Show memory map
    print_memory_map(hVMM, target_pid);
    
    std::cout << "\n=== Usage ===\n";
    std::cout << "Use any 'Image' or 'Private' address from above for memory reading.\n";
    std::cout << "The first module's base address is usually a good choice.\n";
    
    VMMDLL_Close(hVMM);
    
    std::cout << "\nPress ENTER to exit...";
    std::cin.get();
    
    return 0;
}
