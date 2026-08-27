#include "vulkan_device.h"
#include "../utils/logger.h"

#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace PX5 {

namespace {

std::string VkVersion(uint32_t v) {
    char b[32];
    snprintf(b, sizeof(b), "%u.%u.%u",
             VK_API_VERSION_MAJOR(v),
             VK_API_VERSION_MINOR(v),
             VK_API_VERSION_PATCH(v));
    return b;
}

} // namespace

VulkanGpuDevice& VulkanGpuDevice::GetInstance() {
    static VulkanGpuDevice instance;
    return instance;
}

bool VulkanGpuDevice::Initialize() {
    if (m_caps.initialized) return true;

    m_caps = GpuCapabilities{};

    // ---- Stage 1: dynamic loader ------------------------------------
    m_vulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!m_vulkanLib) {
        // v1 faked success here with the string "Mesa Turnip v24.2.0".
        // Honesty: record exact dlerror.
        m_caps.lastError = std::string("dlopen libvulkan.so failed: ") + dlerror();
        PX5_LOGE(LogCategory::GPU, "%s", m_caps.lastError.c_str());
        return false;
    }

    auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(m_vulkanLib, "vkGetInstanceProcAddr"));
    if (!vkGetInstanceProcAddr) {
        m_caps.lastError = "vkGetInstanceProcAddr not found";
        PX5_LOGE(LogCategory::GPU, "%s", m_caps.lastError.c_str());
        return false;
    }

    // ---- Stage 2: instance ------------------------------------------
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "PX5 Foundation";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "PX5Core";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &appInfo;

    auto pfnCreate = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    auto vkEnumerateInstanceVersionPFn =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));

    if (vkEnumerateInstanceVersionPFn) {
        uint32_t apiVer = 0;
        (*vkEnumerateInstanceVersionPFn)(&apiVer);
        m_caps.apiVersionStr = VkVersion(apiVer);
        // Never advertise beyond what the runtime really offers.
        if (appInfo.apiVersion > apiVer) appInfo.apiVersion = apiVer;
    } else {
        m_caps.apiVersionStr = "1.0.x (no vkEnumerateInstanceVersion)";
        appInfo.apiVersion = VK_API_VERSION_1_0;
    }

    if (!pfnCreate ||
        (*pfnCreate)(&ci, nullptr, reinterpret_cast<VkInstance*>(&m_instance))
            != VK_SUCCESS || !m_instance) {
        m_caps.lastError = "vkCreateInstance failed";
        PX5_LOGE(LogCategory::GPU, "%s", m_caps.lastError.c_str());
        return false;
    }

    // ---- Stage 3: physical devices ----------------------------------
    auto pfnEnumDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        vkGetInstanceProcAddr(reinterpret_cast<VkInstance>(m_instance),
                              "vkEnumeratePhysicalDevices"));
    auto pfnGetProps = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        vkGetInstanceProcAddr(reinterpret_cast<VkInstance>(m_instance),
                              "vkGetPhysicalDeviceProperties"));

    uint32_t count = 0;
    if (!pfnEnumDevices ||
        pfnEnumDevices(reinterpret_cast<VkInstance>(m_instance), &count,
                       nullptr) != VK_SUCCESS || count == 0 || !pfnGetProps) {
        m_caps.lastError = "vkEnumeratePhysicalDevices returned no GPUs";
        PX5_LOGE(LogCategory::GPU, "%s", m_caps.lastError.c_str());
        return false;
    }

    std::vector<VkPhysicalDevice> devs(count);
    if (pfnEnumDevices(reinterpret_cast<VkInstance>(m_instance), &count,
                       devs.data()) != VK_SUCCESS) {
        m_caps.lastError = "physical device enumeration failed";
        return false;
    }

    VkPhysicalDeviceProperties props{};
    pfnGetProps(devs[0], &props);

    m_caps.physicalDevices   = count;
    m_caps.deviceName        = props.deviceName;
    m_caps.deviceId          = props.deviceID;
    m_caps.vendorId          = props.vendorID;
    m_caps.driverVersionStr  = VkVersion(props.driverVersion);
    m_caps.initialized       = true;
    m_physDev                = devs[0];

    PX5_LOGI(LogCategory::GPU,
             "Vulkan REAL init OK: %s | API %s | driver %s | vendor=0x%x dev=0x%x (%u GPUs)",
             m_caps.deviceName.c_str(), m_caps.apiVersionStr.c_str(),
             m_caps.driverVersionStr.c_str(), m_caps.vendorId,
             m_caps.deviceId, count);
    return true;
}

void VulkanGpuDevice::Shutdown() {
    if (!m_caps.initialized && !m_instance) return;
    m_caps.initialized = false;
    m_instance = nullptr;
    m_physDev = nullptr;
    if (m_vulkanLib) { dlclose(m_vulkanLib); m_vulkanLib = nullptr; }
    PX5_LOGI(LogCategory::GPU, "Vulkan device shutdown complete");
}

std::string VulkanGpuDevice::GetSummaryString() const {
    if (!m_caps.initialized)
        return "Vulkan: NOT initialized" +
               (m_caps.lastError.empty()
                    ? std::string("")
                    : std::string(" | ") + m_caps.lastError);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Vulkan: ACTIVE | %s | API %s | driver %s | GPUs=%u vendor=0x%04x",
             m_caps.deviceName.c_str(), m_caps.apiVersionStr.c_str(),
             m_caps.driverVersionStr.c_str(), m_caps.physicalDevices,
             m_caps.vendorId);
    return buf;
}

} // namespace PX5
