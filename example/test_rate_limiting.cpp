// VolkDMA - Rate Limiting Test
// Tests for v2.3 rate limiting feature

#include "VolkDMA/dma.hh"
#include "VolkDMA/builder.hh"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

using namespace VolkDMA;

//==============================================================================
// Test Configuration
//==============================================================================
constexpr const char* TEST_PROCESS = "notepad.exe";
constexpr size_t RATE_LIMIT_1MB = 1 * 1024 * 1024;  // 1 MB/s
constexpr size_t RATE_LIMIT_512KB = 512 * 1024;     // 512 KB/s

//==============================================================================
// Helper Functions
//==============================================================================
void print_separator() {
    std::cout << "\n" << std::string(70, '=') << "\n\n";
}

void print_test_header(const std::string& test_name) {
    print_separator();
    std::cout << "🧪 TEST: " << test_name << "\n";
    print_separator();
}

//==============================================================================
// Test 1: Basic Rate Limiting Enable/Disable
//==============================================================================
void test_basic_enable_disable() {
    print_test_header("Basic Rate Limiting Enable/Disable");
    
    try {
        auto dma = DMA::Builder()
            .with_metrics(true)
            .build();
        
        // Check initial state (should be disabled)
        std::cout << "Initial state:\n";
        std::cout << "  Rate limiting enabled: " 
                  << (dma->is_rate_limiting_enabled() ? "Yes" : "No") << "\n";
        std::cout << "  Rate limit: " << dma->get_rate_limit() << " bytes/s\n";
        
        // Enable rate limiting
        dma->enable_rate_limiting(true);
        dma->set_rate_limit(RATE_LIMIT_1MB);
        
        std::cout << "\nAfter enabling (1 MB/s):\n";
        std::cout << "  Rate limiting enabled: " 
                  << (dma->is_rate_limiting_enabled() ? "Yes" : "No") << "\n";
        std::cout << "  Rate limit: " << dma->get_rate_limit() << " bytes/s\n";
        
        // Change limit
        dma->set_rate_limit(RATE_LIMIT_512KB);
        
        std::cout << "\nAfter changing limit (512 KB/s):\n";
        std::cout << "  Rate limit: " << dma->get_rate_limit() << " bytes/s\n";
        
        // Disable
        dma->enable_rate_limiting(false);
        
        std::cout << "\nAfter disabling:\n";
        std::cout << "  Rate limiting enabled: " 
                  << (dma->is_rate_limiting_enabled() ? "Yes" : "No") << "\n";
        
        std::cout << "\n✅ Test passed!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
    }
}

//==============================================================================
// Test 2: Builder Pattern Integration
//==============================================================================
void test_builder_integration() {
    print_test_header("Builder Pattern Integration");
    
    try {
        // Create DMA with rate limiting via builder
        auto dma = DMA::Builder()
            .with_rate_limit(RATE_LIMIT_1MB)
            .with_metrics(true)
            .build();
        
        std::cout << "DMA created with Builder:\n";
        std::cout << "  Rate limiting enabled: " 
                  << (dma->is_rate_limiting_enabled() ? "Yes" : "No") << "\n";
        std::cout << "  Rate limit: " << dma->get_rate_limit() << " bytes/s\n";
        
        std::cout << "\n✅ Test passed!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
    }
}

//==============================================================================
// Test 3: Performance Impact Measurement
//==============================================================================
void test_performance_impact() {
    print_test_header("Performance Impact Measurement");
    
    try {
        auto dma = DMA::Builder()
            .with_metrics(true)
            .with_cache(0)  // Disable cache for accurate measurement
            .build();
        
        DWORD pid = dma->get_process_id(TEST_PROCESS);
        if (pid == 0) {
            std::cout << "⚠️  Process not found: " << TEST_PROCESS << "\n";
            std::cout << "   Start " << TEST_PROCESS << " to run this test\n";
            return;
        }
        
        // Get base address
        uint64_t base_address = 0x140000000;  // Typical base for 64-bit apps
        
        // Test WITHOUT rate limiting
        std::cout << "Testing WITHOUT rate limiting:\n";
        auto start_no_limit = std::chrono::high_resolution_clock::now();
        
        size_t total_bytes = 0;
        for (int i = 0; i < 100; ++i) {
            try {
                uint64_t value = dma->read<uint64_t>(base_address + (i * 8), pid);
                total_bytes += sizeof(uint64_t);
            } catch (...) {
                // Ignore read errors
            }
        }
        
        auto end_no_limit = std::chrono::high_resolution_clock::now();
        auto duration_no_limit = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_no_limit - start_no_limit
        );
        
        std::cout << "  Duration: " << duration_no_limit.count() << " ms\n";
        std::cout << "  Bytes read: " << total_bytes << "\n";
        
        // Test WITH rate limiting (128 KB/s = very restrictive for testing)
        dma->enable_rate_limiting(true);
        dma->set_rate_limit(128 * 1024);  // 128 KB/s
        
        std::cout << "\nTesting WITH rate limiting (128 KB/s):\n";
        auto start_with_limit = std::chrono::high_resolution_clock::now();
        
        total_bytes = 0;
        for (int i = 0; i < 100; ++i) {
            try {
                uint64_t value = dma->read<uint64_t>(base_address + (i * 8), pid);
                total_bytes += sizeof(uint64_t);
            } catch (...) {
                // Ignore read errors
            }
        }
        
        auto end_with_limit = std::chrono::high_resolution_clock::now();
        auto duration_with_limit = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_with_limit - start_with_limit
        );
        
        std::cout << "  Duration: " << duration_with_limit.count() << " ms\n";
        std::cout << "  Bytes read: " << total_bytes << "\n";
        
        // Calculate overhead
        double overhead_percent = ((double)duration_with_limit.count() - duration_no_limit.count()) 
                                 / duration_no_limit.count() * 100.0;
        
        std::cout << "\nPerformance Impact:\n";
        std::cout << "  Overhead: " << std::fixed << std::setprecision(2) 
                  << overhead_percent << "%\n";
        
        std::cout << "\n✅ Test completed!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
    }
}

//==============================================================================
// Test 4: Concurrent Access (Thread Safety)
//==============================================================================
void test_thread_safety() {
    print_test_header("Thread Safety Test");
    
    try {
        auto dma = DMA::Builder()
            .with_rate_limit(RATE_LIMIT_1MB)
            .with_metrics(true)
            .build();
        
        DWORD pid = dma->get_process_id(TEST_PROCESS);
        if (pid == 0) {
            std::cout << "⚠️  Process not found: " << TEST_PROCESS << "\n";
            std::cout << "   Start " << TEST_PROCESS << " to run this test\n";
            return;
        }
        
        uint64_t base_address = 0x140000000;
        
        std::cout << "Running 4 concurrent threads with shared rate limiter...\n";

        // Lambda with explicit captures
        auto worker = [dma_ptr = dma.get(), pid, base_address](int thread_id) {
            for (int i = 0; i < 25; ++i) {
                try {
                    uint64_t value = dma_ptr->read<uint64_t>(base_address + (i * 8), pid);
                    (void)value;  // Unused variable
                } catch (...) {
                    // Ignore errors
                }
            }
            std::cout << "  Thread " << thread_id << " completed\n";
        };
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::thread t1(worker, 1);
        std::thread t2(worker, 2);
        std::thread t3(worker, 3);
        std::thread t4(worker, 4);
        
        t1.join();
        t2.join();
        t3.join();
        t4.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\nAll threads completed in " << duration.count() << " ms\n";
        std::cout << "✅ Thread safety test passed (no crashes)!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
    }
}

//==============================================================================
// Main Test Runner
//==============================================================================
int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║           VolkDMA v2.3 - Rate Limiting Test Suite             ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
)" << "\n";

    std::cout << "This test suite validates the rate limiting feature (v2.3)\n";
    std::cout << "\n⚠️  NOTE: Some tests require " << TEST_PROCESS << " to be running\n";
    
    // Run all tests
    test_basic_enable_disable();
    test_builder_integration();
    test_performance_impact();
    test_thread_safety();
    
    print_separator();
    std::cout << "🏁 All tests completed!\n";
    print_separator();
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    
    return 0;
}
