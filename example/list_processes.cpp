// List all processes on target PC via DMA
// This helps identify what processes are actually available

#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

extern "C" {
    #include "../external/vmm/vmmdll.h"
}

struct ProcessInfo {
    DWORD pid;
    std::string name;
};

int main() {
    std::cout << "===========================================\n";
    std::cout << "  DMA Process Lister - Target PC Scan\n";
    std::cout << "===========================================\n\n";

    // Initialize DMA
    LPCSTR args[] = {
        "",
        "-device",
        "fpga",
        "-waitinitialize"
    };

    std::cout << "[*] Initializing FPGA DMA connection...\n";
    VMM_HANDLE hVMM = VMMDLL_Initialize(4, args);

    if (!hVMM) {
        std::cerr << "[ERROR] Failed to initialize DMA!\n";
        std::cerr << "  - Check FPGA connection\n";
        std::cerr << "  - Run as Administrator\n";
        std::cerr << "  - Verify drivers installed\n";
        return 1;
    }

    std::cout << "[OK] DMA initialized successfully!\n\n";

    // Get process list
    SIZE_T pid_count = 0;
    if (!VMMDLL_PidList(hVMM, nullptr, &pid_count)) {
        std::cerr << "[ERROR] Failed to get process count!\n";
        VMMDLL_Close(hVMM);
        return 1;
    }

    std::cout << "[*] Found " << pid_count << " processes on target PC\n\n";

    DWORD* pid_list = new DWORD[pid_count];
    if (!VMMDLL_PidList(hVMM, pid_list, &pid_count)) {
        std::cerr << "[ERROR] Failed to enumerate processes!\n";
        delete[] pid_list;
        VMMDLL_Close(hVMM);
        return 1;
    }

    // Collect process information
    std::vector<ProcessInfo> processes;
    
    for (SIZE_T i = 0; i < pid_count; i++) {
        VMMDLL_PROCESS_INFORMATION proc_info = {};
        proc_info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
        proc_info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

        if (VMMDLL_ProcessGetInformation(hVMM, pid_list[i], &proc_info, nullptr)) {
            ProcessInfo info;
            info.pid = pid_list[i];
            info.name = proc_info.szName;
            processes.push_back(info);
        }
    }

    delete[] pid_list;

    // Sort by name
    std::sort(processes.begin(), processes.end(), 
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return a.name < b.name;
              });

    // Display results
    std::cout << "┌─────────┬────────────────────────────────┐\n";
    std::cout << "│   PID   │         Process Name           │\n";
    std::cout << "├─────────┼────────────────────────────────┤\n";

    for (const auto& proc : processes) {
        std::cout << "│ " 
                  << std::setw(7) << std::right << proc.pid 
                  << " │ " 
                  << std::setw(30) << std::left << proc.name 
                  << " │\n";
    }

    std::cout << "└─────────┴────────────────────────────────┘\n\n";

    // Show common processes
    std::cout << "Common processes to look for:\n";
    std::vector<std::string> common = {
        "chrome.exe", "notepad.exe", "explorer.exe", 
        "csgo.exe", "valorant.exe", "discord.exe",
        "steam.exe", "System", "lsass.exe"
    };

    for (const auto& name : common) {
        bool found = false;
        for (const auto& proc : processes) {
            std::string lower_proc = proc.name;
            std::string lower_name = name;
            std::transform(lower_proc.begin(), lower_proc.end(), lower_proc.begin(), ::tolower);
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            
            if (lower_proc == lower_name) {
                std::cout << "  [FOUND] " << name << " (PID: " << proc.pid << ")\n";
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "  [  --  ] " << name << " not running\n";
        }
    }

    std::cout << "\nTotal processes: " << processes.size() << "\n";

    VMMDLL_Close(hVMM);
    
    std::cout << "\nPress ENTER to exit...";
    std::cin.get();
    
    return 0;
}
