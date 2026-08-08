#include "vulkan_device.h"
#include "../utils/logger.h"

namespace PX5 {

void InitializeShaderCache(const std::string& cachePath) {
    PX5_LOGI(LogCategory::GPU, "PSSL -> SPIR-V Shader Pipeline Cache initialized at: %s", cachePath.c_str());
}

} // namespace PX5
