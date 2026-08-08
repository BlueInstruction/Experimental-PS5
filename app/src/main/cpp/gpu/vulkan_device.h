#ifndef PX5_VULKAN_DEVICE_H
#define PX5_VULKAN_DEVICE_H

#include <string>

namespace PX5 {

struct AdrenoOptimizationSettings {
    bool useGMEMTileMemory = true;          // TBDR GMEM On-Chip tile memory optimization
    bool enableUBWC = true;                 // Universal Bandwidth Compression
    bool enableSGSR = true;                 // Snapdragon Super Resolution
    float sgsrSharpness = 0.8f;              // SGSR Sharpening filter strength
    bool useDescriptorBuffers = true;       // VK_EXT_descriptor_buffer
    bool useDynamicRendering = true;        // VK_KHR_dynamic_rendering
    bool preferLowLatencyPresenter = true;  // Fast presentation mode
};

struct GpuCapabilities {
    std::string driverName;
    std::string deviceName;
    uint32_t vulkanApiVersion;
    bool bcnTextureSupport;
    bool pipelineCaching;
    bool turnipDriverLoaded;
    AdrenoOptimizationSettings adrenoSettings;
};

class VulkanGpuDevice {
public:
    static VulkanGpuDevice& GetInstance();

    bool Initialize();
    void Shutdown();

    bool InitAdrenotoolsDriver(const std::string& driverDir, const std::string& libName, const std::string& hookLib);
    void SetBCnTextureSupport(bool enabled);
    void SetPipelineCaching(bool enabled);
    void SetAdrenoSettings(const AdrenoOptimizationSettings& settings);

    GpuCapabilities GetCapabilities() const;

private:
    VulkanGpuDevice() = default;
    ~VulkanGpuDevice() = default;

    GpuCapabilities m_caps{ "Adreno (Turnip / Mesa)", "Qualcomm Adreno 750", 0x00403000, true, true, true };
    bool m_initialized = false;
};

} // namespace PX5

#endif // PX5_VULKAN_DEVICE_H
