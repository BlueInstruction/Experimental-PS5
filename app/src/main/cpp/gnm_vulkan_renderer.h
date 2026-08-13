#ifndef PX5_GNM_VULKAN_RENDERER_H
#define PX5_GNM_VULKAN_RENDERER_H

#include <string>
#include <cstdint>

class GnmVulkanRenderer {
public:
    static GnmVulkanRenderer& getInstance();

    bool initVulkanRenderer();
    bool translatePsslToSpirv(const uint8_t* psslBytecode, size_t size);
    std::string getRendererInfo() const;

    uint32_t getTargetFps() const { return m_targetFps; }
    void setTargetFps(uint32_t fps) { m_targetFps = fps; }

    bool isVulkan13Supported() const { return m_vulkan13Supported; }

private:
    GnmVulkanRenderer() = default;
    bool m_initialized = false;
    bool m_vulkan13Supported = true;
    uint32_t m_targetFps = 60;
    uint64_t m_compiledShadersCount = 0;
};

#endif // PX5_GNM_VULKAN_RENDERER_H
