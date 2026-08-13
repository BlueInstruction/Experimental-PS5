#include "memory.h"
#include "../utils/logger.h"

namespace PX5 {

// Page allocation helpers and PS5 memory layout constants
constexpr uint64_t PS5_USER_HEADER_BASE = 0x100000000ULL;
constexpr uint64_t PS5_STACK_BASE       = 0x7FFFF0000ULL;
constexpr uint64_t PS5_TLS_BASE         = 0x800000000ULL;

void PrintMemoryLayoutMap() {
    PX5_LOGI(LogCategory::MEMORY, "=== PS5 Guest Virtual Address Space Map ===");
    PX5_LOGI(LogCategory::MEMORY, "User Executable Header: 0x%llx", (unsigned long long)PS5_USER_HEADER_BASE);
    PX5_LOGI(LogCategory::MEMORY, "User Thread Stack:     0x%llx", (unsigned long long)PS5_STACK_BASE);
    PX5_LOGI(LogCategory::MEMORY, "TLS Base Space:         0x%llx", (unsigned long long)PS5_TLS_BASE);
}

} // namespace PX5
