// Quick Demo: Builder + Logging Integration (v2.9)
// This demonstrates logging without requiring FPGA hardware
// Compile: cl /EHsc demo_logging.cpp /I include /link ArgoSentryDebug.lib

#include <ArgoSentry/builder.hh>
#include <ArgoSentry/logger.hh>
#include <ArgoSentry/log_sinks.hh>
#include <iostream>
#include <thread>
#include <chrono>

using namespace ArgoSentry;

int main() {
    std::cout << "\n========================================\n"
              << "   ArgoSentry v2.9 - Logging Demo\n"
              << "   Builder + Logging Integration\n"
              << "========================================\n\n";

    try {
        // Demo 1: File logging via Builder
        std::cout << "[1] Creating DMA with file logging...\n";
        auto dma1 = DMABuilder()
            .with_logging(LogLevel::INFO, "demo_file.log")
            .build();

        if (dma1) {
            std::cout << "    ✅ DMA created with file logging\n";
            std::cout << "    📝 Check demo_file.log for logs\n\n";
        }

        // Give async logger time to flush
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Demo 2: Console logging via Builder
        std::cout << "[2] Creating DMA with console logging...\n";
        std::cout << "    (Initialization messages below should be colored)\n\n";

        auto dma2 = DMABuilder()
            .with_console_logging(LogLevel::WARN)
            .build();

        if (dma2) {
            std::cout << "\n    ✅ DMA created with console logging\n\n";
        }

        // Demo 3: Both file + console
        std::cout << "[3] Creating DMA with both file and console logging...\n";
        auto dma3 = DMABuilder()
            .with_logging(LogLevel::DEBUG, "demo_both.log")
            .with_console_logging(LogLevel::ERR)
            .build();

        if (dma3) {
            std::cout << "    ✅ DMA created with both loggers\n";
            std::cout << "    📝 Check demo_both.log for detailed logs\n\n";
        }

        // Give async logger time to flush
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Demo 4: Trigger logged operations
        std::cout << "[4] Triggering logged operations (get_process_id)...\n";
        auto dma4 = DMABuilder()
            .with_logging(LogLevel::INFO, "demo_operations.log")
            .build();

        std::cout << "    Calling get_process_id(\"explorer.exe\")...\n";
        DWORD pid = dma4->get_process_id("explorer.exe");
        
        if (pid != 0) {
            std::cout << "    ✅ Process found: explorer.exe (PID " << pid << ")\n";
        } else {
            std::cout << "    ⚠️ Process not found (check logs)\n";
        }
        std::cout << "    📝 Check demo_operations.log for operation logs\n\n";

        // Give async logger time to flush
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Demo 5: Production configuration
        std::cout << "[5] Production-ready DMA with full logging...\n";
        auto dma_prod = DMABuilder::production()
            .with_logging(LogLevel::INFO, "demo_production.log")
            .with_console_logging(LogLevel::ERR)
            .build();

        if (dma_prod) {
            std::cout << "    ✅ Production DMA created\n";
            std::cout << "    Configuration:\n";
            std::cout << "      • Memory map enabled\n";
            std::cout << "      • File logging: INFO → demo_production.log\n";
            std::cout << "      • Console logging: ERROR only\n";
            std::cout << "      • File rotation: 10MB, 5 files\n\n";
        }

        // Final flush
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // Summary
        std::cout << "========================================\n"
                  << "   DEMO COMPLETE ✅\n"
                  << "========================================\n\n";

        std::cout << "Log files created:\n";
        std::cout << "  1. demo_file.log - File logging only\n";
        std::cout << "  2. demo_both.log - Detailed DEBUG logs\n";
        std::cout << "  3. demo_operations.log - Operation logs (get_process_id)\n";
        std::cout << "  4. demo_production.log - Production configuration\n\n";

        std::cout << "Check these files to see:\n";
        std::cout << "  ✅ DMA initialization logs\n";
        std::cout << "  ✅ Memory map loading logs\n";
        std::cout << "  ✅ Process search logs\n";
        std::cout << "  ✅ Error handling logs\n\n";

        std::cout << "Phase 2 (Logging Framework v2.9) - COMPLETE! 🎉\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n[!] ERROR: " << e.what() << "\n\n";
        std::cerr << "Possible causes:\n";
        std::cerr << "  - FPGA not connected (expected for demo)\n";
        std::cerr << "  - Drivers not installed\n";
        std::cerr << "  - Not running as Administrator\n\n";
        return 1;
    }
}
