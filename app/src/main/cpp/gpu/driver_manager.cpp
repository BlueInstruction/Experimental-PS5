#include "driver_manager.h"
#include "../utils/logger.h"

#include <cstdio>

namespace PX5 {

GpuDriverManager& GpuDriverManager::GetInstance() {
    static GpuDriverManager inst;
    return inst;
}

uint32_t GpuDriverManager::RegisterSlot(const std::string& label,
                                        const std::string& soPath) {
    if (label.empty() || soPath.empty()) return 0;
    m_slots.push_back({label, soPath});
    const uint32_t id = static_cast<uint32_t>(m_slots.size());
    PX5_LOGI(LogCategory::GPU, "Driver slot %u registered: %s (%s)",
             id, label.c_str(), soPath.c_str());
    return id;
}

void GpuDriverManager::SetActiveMode(uint32_t mode) {
    if (mode > m_slots.size()) mode = 0;
    m_active = mode;
    PX5_LOGI(LogCategory::GPU,
             "Active GPU driver mode set to %u%s",
             m_active,
             m_active == 0 ? " (system)"
                           : " [out-of-tree injection activates with "
                             "Phase-C adrenotools]");
}

std::string GpuDriverManager::SummaryString() const {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "driver: mode=%u (%s) | slots=%zu | injection=%s",
             m_active,
             m_active == 0 ? "system ICD" : "out-of-tree requested",
             m_slots.size(),
             m_active == 0 ? "n/a"
                           : "PENDING Phase-C adrenotools");
    std::string s = buf;
    for (size_t i = 0; i < m_slots.size(); ++i) {
        s += "\n  slot " + std::to_string(i + 1) + ": " + m_slots[i].label;
    }
    return s;
}

} // namespace PX5
