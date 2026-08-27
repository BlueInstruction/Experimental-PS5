#ifndef PX5_VULKAN_DEVICE_H
#define PX5_VULKAN_DEVICE_H

#include <string>
#include <cstdint>

namespace PX5 {

// Real Vulkan enumeration results (dlopen'd, no fake constants).
struct GpuCapabilities {
    bool        initialized      = false;
    std::string apiVersionStr;              // "1.3.256"
    std::string deviceName;                 // e.g. "Adreno (TM) 750"
    std::string driverVersionStr;
    uint32_t    deviceId          = 0;
    uint32_t    vendorId          = 0;
    uint32_t    physicalDevices   = 0;
    std::string lastError;                  // non-empty => failure detail
};

class VulkanGpuDevice {
public:
    static VulkanGpuDevice& GetInstance();

    // dlopen("libvulkan.so") -> vkCreateInstance -> enumerate devices.
    // Returns false ONLY on genuine runtime failure (never silently).
    bool Initialize();
    void Shutdown();

    const GpuCapabilities& GetCapabilities() const { return m_caps; }
    std::string GetSummaryString() const;

private:
    VulkanGpuDevice() = default;
    ~VulkanGpuDevice() = default;

    GpuCapabilities m_caps;
    void* m_vulkanLib = nullptr;   // dlopen handle
    void* m_instance  = nullptr;   // VkInstance
    void* m_physDev   = nullptr;   // VkPhysicalDevice (borrowed)
};

} // namespace PX5

#endif // PX5_VULKAN_DEVICE_H
