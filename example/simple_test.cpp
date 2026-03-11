// VolkDMA Simple Test Program
// Minimal program to test basic DMA functions with FPGA hardware

#define NOMINMAX
#include <Windows.h>
#include <vmmdll.h>
#include <iostream>
#include <iomanip>
#include <string>

int main() {
    std::cout << "\n=== VolkDMA Simple Test ===\n\n";
    
    // Initialize VMM (MemProcFS)
    std::cout << "[1] Initializing DMA device...\n";

    LPCSTR args[] = { "", "-device", "fpga", "-v" };
    VMM_HANDLE hVMM = VMMDLL_Initialize(4, args);
    
    if (!hVMM) {
        std::cerr << "[ERROR] Failed to initialize DMA!\n";
        std::cerr << "Make sure:\n";
        std::cerr << "  - FPGA is connected\n";
        std::cerr << "  - Running as Administrator\n";
        std::cerr << "  - Drivers installed\n";
        return 1;
    }
    
    std::cout << "[SUCCESS] DMA initialized!\n\n";
    
    // Get process list
    std::cout << "[2] Getting process list...\n";
    
    ULONG64 pPIDs = 0;
    SIZE_T cPIDs = 0;
    
    if (!VMMDLL_PidList(hVMM, NULL, &cPIDs)) {
        std::cerr << "[ERROR] Failed to get process count!\n";
        VMMDLL_Close(hVMM);
        return 1;
    }
    
    DWORD* pids = new DWORD[cPIDs];
    if (!VMMDLL_PidList(hVMM, pids, &cPIDs)) {
        std::cerr << "[ERROR] Failed to get process list!\n";
        delete[] pids;
        VMMDLL_Close(hVMM);
        return 1;
    }
    
    std::cout << "[SUCCESS] Found " << cPIDs << " processes!\n\n";
    
    // Find a specific process
    std::string process_name;
    std::cout << "[3] Enter process name to test (e.g., notepad.exe): ";
    std::getline(std::cin, process_name);
    
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
        std::cerr << "[ERROR] Process not found!\n";
        VMMDLL_Close(hVMM);
        return 1;
    }
    
    std::cout << "[SUCCESS] Found process! PID: " << target_pid << "\n\n";
    
    // Read memory
    std::cout << "[4] Reading memory...\n";
    std::cout << "Enter address (hex, e.g., 0x140000000): 0x";
    uint64_t address;
    std::cin >> std::hex >> address;
    
    uint32_t value = 0;
    if (!VMMDLL_MemRead(hVMM, target_pid, address, (PBYTE)&value, sizeof(value))) {
        std::cerr << "[ERROR] Failed to read memory!\n";
        VMMDLL_Close(hVMM);
        return 1;
    }
    
    std::cout << "[SUCCESS] Read value: 0x" << std::hex << std::setw(8) << std::setfill('0') << value << "\n\n";
    
    // Cleanup
    std::cout << "[5] Cleaning up...\n";
    VMMDLL_Close(hVMM);
    std::cout << "[SUCCESS] Done!\n\n";
    
    std::cout << "Press ENTER to exit...";
    std::cin.ignore();
    std::cin.get();
    
    return 0;
}
