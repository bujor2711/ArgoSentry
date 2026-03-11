// VolkDMA - Internal Complete Header
// This header includes all components in correct order for .cpp files
// DO NOT include this in other headers - only in .cpp implementation files

#pragma once

// Forward declare the namespace first
namespace ArgoSentry {
    class DMA;
}

// Include all complete definitions in dependency order
#include "batch.hh"
#include "metrics.hh"
#include "cache.hh"
#include "memory_layout.hh"
#include "health.hh"

// Now include DMA header which has forward declarations
// After including this, DMA class members will be accessible
#include "dma.hh"

// Now DMA class is fully defined with all its member types available

