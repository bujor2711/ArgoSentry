// Ultra-minimal VMM initialization test
// Tests different device strings to find what works

#include <vmmdll.h>
#include <iostream>
#include <vector>
#include <string>

struct DeviceTest {
    const char* name;
    const char* args[10];
    int argc;
};

int main() {
    std::cout << "=== VMM Device Detection Test ===\n\n";
    
    // Test different device configurations
    std::vector<DeviceTest> tests = {
        {"FPGA Auto", {"", "-device", "fpga"}, 3},
        {"FPGA Verbose", {"", "-device", "fpga", "-v", "-vv"}, 5},
        {"FPGA VFS", {"", "-device", "fpga", "-vfs"}, 4},
        {"USB Auto", {"", "-device", "usb3380"}, 3},
        {"FPGA FTDI", {"", "-device", "fpga://ft601"}, 3},
        {"List All", {"", "-device", "fpga", "-printf"}, 4}
    };
    
    for (size_t i = 0; i < tests.size(); i++) {
        std::cout << "[Test " << (i+1) << "] " << tests[i].name << "...\n";
        std::cout << "  Args: ";
        for (int j = 1; j < tests[i].argc; j++) {
            std::cout << tests[i].args[j] << " ";
        }
        std::cout << "\n";
        
        VMM_HANDLE hVMM = VMMDLL_Initialize(tests[i].argc, (LPSTR*)tests[i].args);
        
        if (hVMM) {
            std::cout << "  Result: SUCCESS!\n";
            std::cout << "  -> This configuration works!\n";
            
            // Try to get process count
            SIZE_T cPIDs = 0;
            if (VMMDLL_PidList(hVMM, NULL, &cPIDs)) {
                std::cout << "  -> Found " << cPIDs << " processes\n";
            }
            
            VMMDLL_Close(hVMM);
            std::cout << "\n  === FOUND WORKING CONFIGURATION ===\n\n";
            return 0;
        } else {
            std::cout << "  Result: FAILED\n\n";
        }
    }
    
    std::cout << "=== All tests failed ===\n\n";
    std::cout << "Possible causes:\n";
    std::cout << "  1. No target PC connected via DMA\n";
    std::cout << "  2. FPGA firmware not flashed correctly\n";
    std::cout << "  3. Wrong device type (need PCILeech FPGA firmware)\n";
    std::cout << "  4. Hardware issue with DMA card\n";
    std::cout << "\nCheck:\n";
    std::cout << "  - Is target PC powered on?\n";
    std::cout << "  - Is DMA card inserted in target PC?\n";
    std::cout << "  - Is USB cable connected?\n";
    std::cout << "  - Does device have PCILeech firmware?\n";
    
    return 1;
}
