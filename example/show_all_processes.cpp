// Quick Process Lister - Shows ALL processes DMA can see
// Compile: cl /EHsc /std:c++20 /I"..\external\vmm" show_all_processes.cpp ..\external\vmm\vmm.lib

#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>

extern "C" {
    #include "../external/vmm/vmmdll.h"
}

int main() {
    std::cout << "DMA Process Scanner - Showing ALL visible processes\n";
    std::cout << "====================================================\n\n";

    // Initialize DMA
    LPCSTR args[] = { "", "-device", "fpga", "-waitinitialize" };
    VMM_HANDLE hVMM = VMMDLL_Initialize(4, args);

    if (!hVMM) {
        std::cerr << "ERROR: Failed to initialize DMA!\n";
        return 1;
    }

    std::cout << "[OK] DMA initialized\n\n";

    // Get process list
    SIZE_T pid_count = 0;
    VMMDLL_PidList(hVMM, nullptr, &pid_count);
    
    DWORD* pid_list = new DWORD[pid_count];
    VMMDLL_PidList(hVMM, pid_list, &pid_count);

    std::cout << "Found " << pid_count << " processes\n\n";
    std::cout << "Searching for your processes...\n";
    std::cout << "================================\n";

    std::vector<std::string> search_for = {
        "chrome.exe", "notepad.exe", "explorer.exe", 
        "cmd.exe", "powershell.exe", "TestDMA.exe"
    };

    for (const auto& name : search_for) {
        bool found = false;
        for (SIZE_T i = 0; i < pid_count; i++) {
            VMMDLL_PROCESS_INFORMATION info = {};
            info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
            info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

            if (VMMDLL_ProcessGetInformation(hVMM, pid_list[i], &info, nullptr)) {
                std::string proc_name = info.szName;
                std::transform(proc_name.begin(), proc_name.end(), proc_name.begin(), ::tolower);
                std::string search_lower = name;
                std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);

                if (proc_name == search_lower) {
                    std::cout << "[FOUND] " << std::left << std::setw(20) << name 
                              << " PID: " << pid_list[i] << "\n";
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            std::cout << "[  --  ] " << std::left << std::setw(20) << name << " NOT FOUND\n";
        }
    }

    std::cout << "\n\nFull Process List (first 50):\n";
    std::cout << "==============================\n";
    
    int count = 0;
    for (SIZE_T i = 0; i < pid_count && count < 50; i++) {
        VMMDLL_PROCESS_INFORMATION info = {};
        info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
        info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

        if (VMMDLL_ProcessGetInformation(hVMM, pid_list[i], &info, nullptr)) {
            std::cout << std::setw(7) << pid_list[i] << " - " << info.szName << "\n";
            count++;
        }
    }

    delete[] pid_list;
    VMMDLL_Close(hVMM);

    std::cout << "\nPress ENTER to exit...";
    std::cin.get();
    return 0;
}
