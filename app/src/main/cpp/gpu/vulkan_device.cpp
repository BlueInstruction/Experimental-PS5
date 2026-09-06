// The Vulkan backend TU is the ONE sanctioned place where Vulkan code and
// GPU IR meet (IR in, Vulkan out — docs/gpu.md layer chain). Defining this
// before any include disarms gpu_ir.h's Vulkan guard FOR THIS TU ONLY;
// every other context keeps the guard armed.
#define PX5_GPU_BACKEND_CONSUMER 1

#include "vulkan_device.h"
#include "driver_manager.h"
#include "vulkan_backend.h"
#include "../core/settings.h"
#include "../utils/logger.h"
#include "../utils/breadcrumbs.h"
#include "../utils/evidence.h"

#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>

#include <vulkan/vulkan_android.h>      // VkAndroidSurfaceCreateInfoKHR
#include <android/native_window.h>

namespace PX5 {

namespace {

std::string VkVersion(uint32_t v) {
    char b[32];
    snprintf(b, sizeof(b), "%u.%u.%u",
             VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v),
             VK_API_VERSION_PATCH(v));
    return b;
}

// Memory-type selection honouring EngineSettings::vramUsageMode. Runs on the
// caller's thread against freshly-queried device properties; every branch
// picks a type that satisfies reqs, so this can never return an invalid
// index when the device reports at least one usable type.
uint32_t SelectImageMemoryType(
        const VkPhysicalDeviceMemoryProperties& mp,
        uint32_t allowedBits) {
    const int mode = EngineSettings::vramUsageMode.load();

    // conservative (0): first HOST_VISIBLE type wins; device-local-only
    // types are skipped so allocations land in shared memory.
    if (mode == 0) {
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((allowedBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags &
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) return i;
    }
    // aggressive (2): DEVICE_LOCAL required; no fallback to non-local.
    if (mode == 2) {
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((allowedBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags &
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) return i;
        // No DEVICE_LOCAL type satisfies reqs (should not happen for images
        // on any real device): fall through so the caller gets a valid type
        // instead of a guaranteed allocation failure.
    }
    // balanced (1) + aggressive fallback: DEVICE_LOCAL preferred, any type
    // acceptable.
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((allowedBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) return i;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if (allowedBits & (1u << i)) return i;
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
// RenderStats helpers
// ---------------------------------------------------------------------------
std::string VulkanGpuDevice::RenderStats::LastError() const {
    std::lock_guard<std::mutex> lk(lock);
    return error;
}
void VulkanGpuDevice::RenderStats::SetError(const std::string& e) {
    std::lock_guard<std::mutex> lk(lock);
    error = e;
}

VulkanGpuDevice& VulkanGpuDevice::GetInstance() {
    static VulkanGpuDevice instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Layer 1 — loader / instance / physical device
// ---------------------------------------------------------------------------
bool VulkanGpuDevice::Initialize() {
    if (m_caps.initialized) return true;
    m_caps = GpuCapabilities{};

    // ---- Stage 1: dynamic loader --------------------------------------
    // Real driver selection: mode 0 = system ICD, >0 = Turnip/other driver
    // loaded through libadrenotools (linker-namespace hook), Winlator-style.
    m_vulkanLib = PX5::GpuDriverManager::GetInstance()
                      .OpenHostVulkanLibrary(RTLD_NOW | RTLD_LOCAL);
    if (!m_vulkanLib) {
        m_caps.lastError = std::string("dlopen libvulkan.so failed: ") + dlerror();
        PX5_LOGE(LogCategory::GPU, "%s", m_caps.lastError.c_str());
        return false;
    }

    auto vkGIPA = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(m_vulkanLib, "vkGetInstanceProcAddr"));
    if (!vkGIPA) {
        m_caps.lastError = "vkGetInstanceProcAddr not found";
        return false;
    }

    // ---- Stage 2: instance (surface extensions ALWAYS on so the on-screen
    //      path needs no instance rebuild) -------------------------------
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName  = "PX5";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    appInfo.pEngineName       = "PX5Core";
    appInfo.engineVersion     = VK_MAKE_VERSION(0, 2, 0);

    uint32_t apiVer = VK_API_VERSION_1_0;
    auto pfnEnumInstVer = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGIPA(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (pfnEnumInstVer) { pfnEnumInstVer(&apiVer); }
    appInfo.apiVersion   = apiVer;
    m_caps.apiVersionStr = VkVersion(apiVer);

    const char* instExts[] = { "VK_KHR_surface", "VK_KHR_android_surface" };
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo       = &appInfo;
    ci.enabledExtensionCount  = 2;
    ci.ppEnabledExtensionNames= instExts;

    auto pfnCreate = reinterpret_cast<PFN_vkCreateInstance>(
        vkGIPA(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!pfnCreate ||
        (*pfnCreate)(&ci, nullptr,
                     reinterpret_cast<VkInstance*>(&m_instance)) != VK_SUCCESS ||
        !m_instance) {
        m_caps.lastError = "vkCreateInstance failed";
        PX5_LOGE(LogCategory::GPU, "%s", m_caps.lastError.c_str());
        return false;
    }

    // The Android loader binds an ICD on its first instance-level call, not
    // at dlopen — so this is the first point where the binding exists and a
    // check means something. Prove the chosen custom driver is really
    // mapped, or say so loudly: a run that quietly renders through the
    // system driver looks exactly like a successful injection.
    PX5::GpuDriverManager::GetInstance().VerifyActiveDriverMapped();

    // ---- Stage 3: physical devices ------------------------------------
    auto pfnEnumDevs = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        vkGIPA(reinterpret_cast<VkInstance>(m_instance),
               "vkEnumeratePhysicalDevices"));
    auto pfnProps    = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        vkGIPA(reinterpret_cast<VkInstance>(m_instance),
               "vkGetPhysicalDeviceProperties"));

    uint32_t count = 0;
    if (!pfnEnumDevs || pfnEnumDevs(
            reinterpret_cast<VkInstance>(m_instance), &count, nullptr)
                != VK_SUCCESS || count == 0 || !pfnProps) {
        m_caps.lastError = "no physical devices enumerated";
        return false;
    }
    std::vector<VkPhysicalDevice> devs(count);
    pfnEnumDevs(reinterpret_cast<VkInstance>(m_instance), &count, devs.data());

    VkPhysicalDeviceProperties props{};
    pfnProps(devs[0], &props);
    m_caps.physicalDevices  = count;
    m_caps.deviceName       = props.deviceName;
    m_caps.deviceId         = props.deviceID;
    m_caps.vendorId         = props.vendorID;
    m_caps.driverVersionStr = VkVersion(props.driverVersion);
    m_caps.initialized      = true;
    m_physDev               = devs[0];

    PX5_LOGI(LogCategory::GPU,
             "Vulkan REAL init OK: %s | API %s | driver %s | vendor=0x%x "
             "dev=0x%x (%u GPUs)",
             m_caps.deviceName.c_str(), m_caps.apiVersionStr.c_str(),
             m_caps.driverVersionStr.c_str(), m_caps.vendorId,
             m_caps.deviceId, count);
    return true;
}

void VulkanGpuDevice::Shutdown() {
    StopRenderLoop();

    std::lock_guard<std::mutex> gpuLock(m_gpuMutex);
    DestroySwapchainLocked();

    if (m_window) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }
    if (m_device != VK_NULL_HANDLE && m_tbl.QueueWaitIdle)
        m_tbl.QueueWaitIdle(m_queue);

    if (m_device != VK_NULL_HANDLE) {
        auto vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
            dlsym(m_vulkanLib, "vkDestroyDevice"));
        if (vkDestroyDevice) vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_stats.deviceReady.store(false);

    if (m_surface != VK_NULL_HANDLE) {
        auto vkDestroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            dlsym(m_vulkanLib, "vkDestroySurfaceKHR"));
        if (vkDestroySurface)
            vkDestroySurface(reinterpret_cast<VkInstance>(m_instance),
                             m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_instance) {
        auto vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
            dlsym(m_vulkanLib, "vkDestroyInstance"));
        if (vkDestroyInstance)
            vkDestroyInstance(reinterpret_cast<VkInstance>(m_instance),
                              nullptr);
        m_instance = nullptr;
        m_physDev  = nullptr;
    }
    m_caps.initialized = false;
    if (m_vulkanLib) { dlclose(m_vulkanLib); m_vulkanLib = nullptr; }
    PX5_LOGI(LogCategory::GPU, "Vulkan shutdown complete");
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

// ---------------------------------------------------------------------------
// Layer 2 — logical device
// ---------------------------------------------------------------------------
uint32_t VulkanGpuDevice::FindGraphicsQueueFamily(VkPhysicalDevice dev) {
    auto pfnFamilies =
        reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
            dlsym(m_vulkanLib, "vkGetPhysicalDeviceQueueFamilyProperties"));
    if (!pfnFamilies) return 0xFFFFFFFFu;

    uint32_t n = 0;
    pfnFamilies(dev, &n, nullptr);
    if (n == 0) return 0xFFFFFFFFu;

    std::vector<VkQueueFamilyProperties> fams(n);
    pfnFamilies(dev, &n, fams.data());

    for (uint32_t i = 0; i < n; ++i)
        if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
    return 0xFFFFFFFFu;
}

bool VulkanGpuDevice::BuildDeviceTable(VkDevice dev) {
    auto vkGDPA = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        dlsym(m_vulkanLib, "vkGetDeviceProcAddr"));
    if (!vkGDPA) {
        m_stats.SetError("vkGetDeviceProcAddr unavailable");
        return false;
    }

#define PX5_DEV_FN(field, name)                                                \
    do {                                                                       \
        m_tbl.field = reinterpret_cast<decltype(m_tbl.field)>(                 \
            vkGDPA(dev, name));                                                \
        if (!m_tbl.field) {                                                    \
            m_stats.SetError(std::string("missing device fn ") + name);        \
            return false;                                                      \
        }                                                                      \
    } while (0)

    PX5_DEV_FN(GetDeviceQueue,         "vkGetDeviceQueue");
    PX5_DEV_FN(CreateCommandPool,      "vkCreateCommandPool");
    PX5_DEV_FN(AllocateCommandBuffers, "vkAllocateCommandBuffers");
    PX5_DEV_FN(BeginCommandBuffer,     "vkBeginCommandBuffer");
    PX5_DEV_FN(CmdClearColorImage,     "vkCmdClearColorImage");
    PX5_DEV_FN(CmdPipelineBarrier,     "vkCmdPipelineBarrier");
    PX5_DEV_FN(EndCommandBuffer,       "vkEndCommandBuffer");
    PX5_DEV_FN(ResetCommandBuffer,     "vkResetCommandBuffer");
    PX5_DEV_FN(CreateFence,            "vkCreateFence");
    PX5_DEV_FN(DestroyFence,           "vkDestroyFence");
    PX5_DEV_FN(CreateSemaphore,        "vkCreateSemaphore");
    PX5_DEV_FN(DestroySemaphore,       "vkDestroySemaphore");
    PX5_DEV_FN(CreateImage,            "vkCreateImage");
    PX5_DEV_FN(DestroyImage,           "vkDestroyImage");
    PX5_DEV_FN(GetImageMemReqs,        "vkGetImageMemoryRequirements");
    PX5_DEV_FN(AllocateMemory,         "vkAllocateMemory");
    PX5_DEV_FN(FreeMemory,             "vkFreeMemory");
    PX5_DEV_FN(BindImageMemory,        "vkBindImageMemory");
    PX5_DEV_FN(DestroyCommandPool,     "vkDestroyCommandPool");
    PX5_DEV_FN(QueueSubmit,            "vkQueueSubmit");
    PX5_DEV_FN(QueueWaitIdle,          "vkQueueWaitIdle");
    PX5_DEV_FN(WaitForFences,          "vkWaitForFences");
    PX5_DEV_FN(ResetFences,            "vkResetFences");

#undef PX5_DEV_FN

    // ---- swapchain entry points: OPTIONAL by spec -----------------------
    // vkGetDeviceProcAddr returns NULL for an extension function unless the
    // extension was enabled AT DEVICE CREATION. Before v1.18 the device was
    // created extension-less and these four lines HARD-FAILED here
    // ("missing device fn vkCreateSwapchainKHR" — the exact line in the
    // 2026-08-30 device video), killing rendering on every driver.
    // EnsureLogicalDeviceUnlocked now enables VK_KHR_swapchain when the
    // device reports it; this block records what actually resolved instead
    // of failing the whole device, so an offscreen-only device stays usable
    // with an honest flag, and CreateSwapchainLocked reports the truth.
    {
        auto resolve = [&](void** slot, const char* name) {
            *slot = reinterpret_cast<void*>(vkGDPA(dev, name));
        };
        resolve(reinterpret_cast<void**>(&m_tbl.CreateSwapchain),    "vkCreateSwapchainKHR");
        resolve(reinterpret_cast<void**>(&m_tbl.DestroySwapchain),   "vkDestroySwapchainKHR");
        resolve(reinterpret_cast<void**>(&m_tbl.GetSwapchainImages), "vkGetSwapchainImagesKHR");
        resolve(reinterpret_cast<void**>(&m_tbl.AcquireNextImage),   "vkAcquireNextImageKHR");
        resolve(reinterpret_cast<void**>(&m_tbl.QueuePresent),       "vkQueuePresentKHR");

        const bool all = m_tbl.CreateSwapchain && m_tbl.DestroySwapchain &&
                         m_tbl.GetSwapchainImages && m_tbl.AcquireNextImage &&
                         m_tbl.QueuePresent;
        if (all) {
            m_swapchainExtAvailable = true;
        } else {
            m_swapchainExtAvailable = false;
            PX5_LOGW(LogCategory::GPU,
                     "swapchain entry points unresolved on this device "
                     "(VK_KHR_swapchain not enabled/reported) — the "
                     "on-screen path will report it honestly");
        }
    }
    return true;
}

bool VulkanGpuDevice::EnsureLogicalDevice() {
    std::lock_guard<std::mutex> lk(m_gpuMutex);
    return EnsureLogicalDeviceUnlocked();
}

// Caller must hold m_gpuMutex.
bool VulkanGpuDevice::EnsureLogicalDeviceUnlocked() {
    if (m_device != VK_NULL_HANDLE && m_tbl.GetDeviceQueue) return true;
    if (!m_caps.initialized && !Initialize()) {
        m_stats.SetError("instance init failed");
        return false;
    }

    m_queueFamily = FindGraphicsQueueFamily(
        reinterpret_cast<VkPhysicalDevice>(m_physDev));
    if (m_queueFamily == 0xFFFFFFFFu) {
        m_stats.SetError("no graphics queue family");
        return false;
    }

    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = m_queueFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    // ---- v1.18 ROOT CAUSE FIX (deterministic, driver-independent) -------
    // The device was created with ZERO device extensions, so per the Vulkan
    // spec vkGetDeviceProcAddr returned NULL for every swapchain entry
    // point: BuildDeviceTable died on "missing device fn
    // vkCreateSwapchainKHR" — the exact line in the 2026-08-30 device
    // video — EnsureLogicalDevice failed, no surface attach, no swapchain,
    // frames=0 forever, on ANY driver including the stock one. Extension
    // trampolines are only wired for extensions enabled AT DEVICE CREATION.
    // The on-screen path needs VK_KHR_swapchain; query the device and
    // enable it when reported (every Android GPU exposes it), and say so
    // loudly when it does not.
    std::vector<const char*> devExts;
    m_swapchainExtAvailable = false;
    auto pfnDevExts = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        dlsym(m_vulkanLib, "vkEnumerateDeviceExtensionProperties"));
    if (pfnDevExts) {
        uint32_t n = 0;
        if (pfnDevExts(reinterpret_cast<VkPhysicalDevice>(m_physDev),
                       nullptr, &n, nullptr) == VK_SUCCESS && n > 0) {
            std::vector<VkExtensionProperties> exts(n);
            if (pfnDevExts(reinterpret_cast<VkPhysicalDevice>(m_physDev),
                           nullptr, &n, exts.data()) == VK_SUCCESS) {
                bool haveSwap = false;
                for (const auto& e : exts)
                    if (std::strcmp(e.extensionName, "VK_KHR_swapchain") == 0)
                        { haveSwap = true; break; }
                if (haveSwap) {
                    devExts.push_back("VK_KHR_swapchain");
                    m_swapchainExtAvailable = true;   // confirmed by BuildDeviceTable below
                }
            }
        }
    }
    if (m_swapchainExtAvailable) {
        PX5_LOGI(LogCategory::GPU,
                 "Logical device will enable VK_KHR_swapchain "
                 "(v1.18 fix — the extension-less device could never render)");
    } else {
        PX5_LOGW(LogCategory::GPU,
                 "VK_KHR_swapchain NOT reported by this physical device — "
                 "on-screen rendering is impossible here; offscreen proofs "
                 "still work");
    }

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos    = &qci;
    dci.enabledExtensionCount   = static_cast<uint32_t>(devExts.size());
    dci.ppEnabledExtensionNames = devExts.empty() ? nullptr : devExts.data();

    auto vkCreateDev = reinterpret_cast<PFN_vkCreateDevice>(
        dlsym(m_vulkanLib, "vkCreateDevice"));
    if (!vkCreateDev || vkCreateDev(
            reinterpret_cast<VkPhysicalDevice>(m_physDev), &dci, nullptr,
            &m_device) != VK_SUCCESS) {
        m_stats.SetError("vkCreateDevice failed");
        return false;
    }
    if (!BuildDeviceTable(m_device)) return false;

    // vkGetDeviceQueue returns void — the only honest guard is the output
    // handle. v1.15: was unchecked; a null queue would later reach
    // QueueSubmit and fault inside the driver (si_addr=0x0).
    m_tbl.GetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
    if (m_queue == VK_NULL_HANDLE) {
        m_stats.SetError("GetDeviceQueue produced a null queue");
        return false;
    }
    m_stats.deviceReady.store(true);
    PX5_LOGI(LogCategory::GPU, "Logical device ready (qFamily=%u)",
             m_queueFamily);
    return true;
}

// ---------------------------------------------------------------------------
// Offscreen submission proof (no window needed — full submission path)
// ---------------------------------------------------------------------------
bool VulkanGpuDevice::RunOffscreenClearProof(std::string& detailOut) {
    std::lock_guard<std::mutex> lk(m_gpuMutex);
    return RunOffscreenProofLocked(detailOut);
}

bool VulkanGpuDevice::RunOffscreenProofLocked(std::string& err) {
    // Every step below stamps a breadcrumb BEFORE executing. If the driver
    // hangs or faults inside one of them, the crash report (or the next
    // paste) names the exact step instead of a silent death.
    Breadcrumb::Set("gpu.proof: enter");
    if (!EnsureLogicalDeviceUnlocked()) {
        err = "device: " + m_stats.LastError();
        return false;
    }
    Breadcrumb::Set("gpu.proof: device ready");

    constexpr uint32_t W = 64, H = 64;

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent        = {W, H, 1};
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage img = VK_NULL_HANDLE;
    Breadcrumb::Set("gpu.proof: create_image");
    if (m_tbl.CreateImage(m_device, &ici, nullptr, &img) != VK_SUCCESS) {
        err = "CreateImage failed";
        return false;
    }
    Breadcrumb::Set("gpu.proof: image created");

    bool ok = false;
    VkDeviceMemory mem  = VK_NULL_HANDLE;
    VkCommandPool pool  = VK_NULL_HANDLE;
    VkFence fence       = VK_NULL_HANDLE;

    do {
        VkMemoryRequirements reqs{};
        m_tbl.GetImageMemReqs(m_device, img, &reqs);

        auto pfnMemProps =
            reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
                dlsym(m_vulkanLib,
                      "vkGetPhysicalDeviceMemoryProperties"));
        if (!pfnMemProps) { err = "mem-properties fn missing"; break; }
        VkPhysicalDeviceMemoryProperties mp{};
        pfnMemProps(reinterpret_cast<VkPhysicalDevice>(m_physDev), &mp);

        uint32_t memType = SelectImageMemoryType(mp, reqs.memoryTypeBits);
        Breadcrumb::Set("gpu.proof: memtype=%u mode=%d",
                        memType, EngineSettings::vramUsageMode.load());

        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize  = reqs.size;
        mai.memoryTypeIndex = memType;

        Breadcrumb::Set("gpu.proof: alloc_memory");
        if (m_tbl.AllocateMemory(m_device, &mai, nullptr, &mem)
                != VK_SUCCESS) { err = "AllocateMemory failed"; break; }
        Breadcrumb::Set("gpu.proof: bind_image_memory");
        if (m_tbl.BindImageMemory(m_device, img, mem, 0) != VK_SUCCESS) {
            err = "BindImageMemory failed"; break;
        }
        Breadcrumb::Set("gpu.proof: image bound");

        VkCommandPoolCreateInfo pci{
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.flags            =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = m_queueFamily;
        Breadcrumb::Set("gpu.proof: create_cmd_pool");
        if (m_tbl.CreateCommandPool(m_device, &pci, nullptr, &pool)
                != VK_SUCCESS) { err = "CreateCommandPool failed"; break; }

        VkCommandBufferAllocateInfo cai{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool        = pool;
        cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        Breadcrumb::Set("gpu.proof: alloc_cmd_buffer");
        if (m_tbl.AllocateCommandBuffers(m_device, &cai, &cb)
                != VK_SUCCESS) { err = "AllocateCommandBuffers failed"; break; }

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        Breadcrumb::Set("gpu.proof: create_fence");
        // v1.15: was unchecked — a null fence passed to QueueSubmit faults
        // inside the driver (si_addr=0x0). Named failure now.
        if (m_tbl.CreateFence(m_device, &fci, nullptr, &fence)
                != VK_SUCCESS || fence == VK_NULL_HANDLE) {
            err = "CreateFence failed"; break;
        }

        VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        ib.image            = img;
        ib.srcAccessMask    = 0;
        ib.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
        ib.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        ib.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        ib.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkCommandBufferBeginInfo bi{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkClearColorValue color{};
        color.float32[0] = 0.08f;
        color.float32[1] = 0.72f;
        color.float32[2] = 0.70f;
        color.float32[3] = 1.0f;
        const VkImageSubresourceRange range{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        ok = true;
        Breadcrumb::Set("gpu.proof: record");
        do {
            if (m_tbl.BeginCommandBuffer(cb, &bi) != VK_SUCCESS) {
                err = "BeginCommandBuffer failed"; ok = false; break;
            }
            m_tbl.CmdPipelineBarrier(cb,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                     0, nullptr, 0, nullptr, 1, &ib);
            m_tbl.CmdClearColorImage(
                cb, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &color, 1, &range);
            if (m_tbl.EndCommandBuffer(cb) != VK_SUCCESS) {
                err = "EndCommandBuffer failed"; ok = false; break;
            }

            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1;
            si.pCommandBuffers    = &cb;
            Breadcrumb::Set("gpu.proof: submit");
            bool submitted = false;
            {
                // Queue external-sync contract: the render thread may be
                // submitting on the same m_queue concurrently.
                std::lock_guard<std::mutex> qlk(m_queueMutex);
                submitted =
                    m_tbl.QueueSubmit(m_queue, 1, &si, fence) == VK_SUCCESS;
            }
            if (!submitted) { err = "QueueSubmit failed"; ok = false; break; }
            Breadcrumb::Set("gpu.proof: fence_wait");
            if (m_tbl.WaitForFences(m_device, 1, &fence, VK_TRUE,
                                    3000000000ull) != VK_SUCCESS) {
                err = "fence timeout (3 s)"; ok = false; break;
            }
        } while (false);
    } while (false);

    if (fence != VK_NULL_HANDLE)
        m_tbl.DestroyFence(m_device, fence, nullptr);
    if (pool != VK_NULL_HANDLE)
        m_tbl.DestroyCommandPool(m_device, pool, nullptr);
    if (mem != VK_NULL_HANDLE)
        m_tbl.FreeMemory(m_device, mem, nullptr);
    m_tbl.DestroyImage(m_device, img, nullptr);
    Breadcrumb::Set("gpu.proof: done ok=%d", ok ? 1 : 0);

    if (ok) {
        err = "64x64 clear submitted + fenced OK";
        PX5_LOGI(LogCategory::GPU, "Offscreen proof: %s", err.c_str());

    } else {
        PX5_LOGE(LogCategory::GPU, "Offscreen proof FAILED: %s", err.c_str());
    }
    return ok;
}

// ---------------------------------------------------------------------------
// v1.18 — self-contained proof (fork-safe): fresh instance -> device ->
// clear submit -> full teardown, ALL LOCAL. The fork-isolated GPU-proof
// child must never submit on the singleton's queue: that queue and its
// driver-internal state were created by the PARENT process, and a
// forked child submitting on parent-created driver state is exactly the
// hazard behind the 2026-08-30 device evidence ("CRASHED in isolated
// child (signal 11)", si_addr=0x0 right after the gpu.proof:submit
// breadcrumb). A fresh VkDevice in the child opens its OWN drm render
// node fd — a clean kernel context that shares nothing with the parent.
// Touches no VulkanGpuDevice member except the m_vulkanLib HANDLE READ
// (a mapped-library pointer, safe across fork; dlopen would hand back
// the identical address anyway).
// ---------------------------------------------------------------------------
bool VulkanGpuDevice::RunSelfContainedProof(std::string& detailOut) {
    void* lib = m_vulkanLib ? m_vulkanLib
                            : dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        detailOut = std::string("dlopen libvulkan.so failed: ") + dlerror();
        return false;
    }

    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(lib, "vkGetInstanceProcAddr"));
    if (!gipa) { detailOut = "vkGetInstanceProcAddr missing"; return false; }

    Breadcrumb::Set("gpu.proof: enter (self-contained)");

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "PX5-proof";
    appInfo.apiVersion       = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &appInfo;          // offscreen: no surface exts
    VkInstance inst = VK_NULL_HANDLE;
    auto pfnCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        gipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!pfnCreateInstance ||
        pfnCreateInstance(&ici, nullptr, &inst) != VK_SUCCESS || !inst) {
        detailOut = "self-contained: vkCreateInstance failed";
        return false;
    }
    Breadcrumb::Set("gpu.proof: instance ready");

    auto pfnEnumDevs = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        gipa(inst, "vkEnumeratePhysicalDevices"));
    auto pfnProps = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        gipa(inst, "vkGetPhysicalDeviceProperties"));
    auto pfnFams = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        gipa(inst, "vkGetPhysicalDeviceQueueFamilyProperties"));
    auto pfnMemProps = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        gipa(inst, "vkGetPhysicalDeviceMemoryProperties"));
    auto pfnCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        gipa(inst, "vkCreateDevice"));
    if (!pfnEnumDevs || !pfnProps || !pfnFams || !pfnMemProps ||
        !pfnCreateDevice) {
        detailOut = "self-contained: instance-level fns missing";
        return false;
    }

    uint32_t nd = 0;
    if (pfnEnumDevs(inst, &nd, nullptr) != VK_SUCCESS || nd == 0) {
        detailOut = "self-contained: no physical devices";
        return false;
    }
    std::vector<VkPhysicalDevice> devs(nd);
    pfnEnumDevs(inst, &nd, devs.data());
    VkPhysicalDevice pd = devs[0];
    VkPhysicalDeviceProperties props{};
    pfnProps(pd, &props);
    Breadcrumb::Set("gpu.proof: physical device ready");

    uint32_t nf = 0;
    pfnFams(pd, &nf, nullptr);
    if (nf == 0) { detailOut = "self-contained: no queue families"; return false; }
    std::vector<VkQueueFamilyProperties> fams(nf);
    pfnFams(pd, &nf, fams.data());
    uint32_t gfx = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < nf; ++i)
        if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { gfx = i; break; }
    if (gfx == 0xFFFFFFFFu) {
        detailOut = "self-contained: no graphics queue family";
        return false;
    }

    // Fresh logical device — extension-less is CORRECT here: this proof is
    // offscreen-only, and the v1.18 on-screen swapchain fix lives in
    // EnsureLogicalDeviceUnlocked, not in the diagnostic stack.
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = gfx;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos    = &qci;
    VkDevice dev = VK_NULL_HANDLE;
    if (pfnCreateDevice(pd, &dci, nullptr, &dev) != VK_SUCCESS || !dev) {
        detailOut = "self-contained: vkCreateDevice failed";
        return false;
    }
    Breadcrumb::Set("gpu.proof: fresh device ready");

    auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        gipa(inst, "vkGetDeviceProcAddr"));
#define PX5_P(fn, name) \
    auto fn = reinterpret_cast<PFN_vk##fn>(gdpa ? gdpa(dev, name) : nullptr)
    PX5_P(GetDeviceQueue,         "vkGetDeviceQueue");
    PX5_P(CreateImage,            "vkCreateImage");
    // (GetImageMemoryRequirements is resolved separately below — its
    // shorthand name would break the PFN_vk##fn token paste.)
    PX5_P(AllocateMemory,         "vkAllocateMemory");
    PX5_P(BindImageMemory,        "vkBindImageMemory");
    PX5_P(CreateCommandPool,      "vkCreateCommandPool");
    PX5_P(AllocateCommandBuffers, "vkAllocateCommandBuffers");
    PX5_P(BeginCommandBuffer,     "vkBeginCommandBuffer");
    PX5_P(CmdPipelineBarrier,     "vkCmdPipelineBarrier");
    PX5_P(CmdClearColorImage,     "vkCmdClearColorImage");
    PX5_P(EndCommandBuffer,       "vkEndCommandBuffer");
    PX5_P(QueueSubmit,            "vkQueueSubmit");
    PX5_P(WaitForFences,          "vkWaitForFences");
    PX5_P(CreateFence,            "vkCreateFence");
    PX5_P(DestroyFence,           "vkDestroyFence");
    PX5_P(DestroyCommandPool,     "vkDestroyCommandPool");
    PX5_P(FreeMemory,             "vkFreeMemory");
    PX5_P(DestroyImage,           "vkDestroyImage");
    PX5_P(DestroyDevice,          "vkDestroyDevice");
#undef PX5_P
    auto GetImageMemReqs = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
        gdpa ? gdpa(dev, "vkGetImageMemoryRequirements") : nullptr);

    bool ok = false;
    if (!GetDeviceQueue || !CreateImage || !GetImageMemReqs ||
        !AllocateMemory || !BindImageMemory || !CreateCommandPool ||
        !AllocateCommandBuffers || !BeginCommandBuffer ||
        !CmdPipelineBarrier || !CmdClearColorImage || !EndCommandBuffer ||
        !QueueSubmit || !WaitForFences || !CreateFence || !DestroyFence ||
        !DestroyCommandPool || !FreeMemory || !DestroyImage ||
        !DestroyDevice) {
        detailOut = "self-contained: device-level fns missing";
    } else {
        VkQueue queue = VK_NULL_HANDLE;
        GetDeviceQueue(dev, gfx, 0, &queue);
        ok = queue != VK_NULL_HANDLE;
        detailOut = ok ? "" : "self-contained: GetDeviceQueue null";

        constexpr uint32_t W = 64, H = 64;
        VkImageCreateInfo ici2{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici2.imageType     = VK_IMAGE_TYPE_2D;
        ici2.format        = VK_FORMAT_R8G8B8A8_UNORM;
        ici2.extent        = {W, H, 1};
        ici2.mipLevels     = 1;
        ici2.arrayLayers   = 1;
        ici2.samples       = VK_SAMPLE_COUNT_1_BIT;
        ici2.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ici2.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ici2.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage img = VK_NULL_HANDLE;
        if (ok && CreateImage(dev, &ici2, nullptr, &img) != VK_SUCCESS) {
            ok = false; detailOut = "self-contained: CreateImage failed";
        }

        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        if (ok) {
            VkMemoryRequirements reqs{};
            GetImageMemReqs(dev, img, &reqs);
            VkPhysicalDeviceMemoryProperties mp{};
            pfnMemProps(pd, &mp);
            VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            mai.allocationSize  = reqs.size;
            mai.memoryTypeIndex = SelectImageMemoryType(mp, reqs.memoryTypeBits);
            if (AllocateMemory(dev, &mai, nullptr, &mem) != VK_SUCCESS ||
                BindImageMemory(dev, img, mem, 0) != VK_SUCCESS) {
                ok = false; detailOut = "self-contained: memory alloc/bind failed";
            }
        }
        if (ok) {
            VkCommandPoolCreateInfo pci{
                VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pci.queueFamilyIndex = gfx;
            if (CreateCommandPool(dev, &pci, nullptr, &pool) != VK_SUCCESS) {
                ok = false; detailOut = "self-contained: CreateCommandPool failed";
            }
        }
        if (ok) {
            VkCommandBufferAllocateInfo cai{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cai.commandPool        = pool;
            cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cai.commandBufferCount = 1;
            VkCommandBuffer cb = VK_NULL_HANDLE;
            VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            if (AllocateCommandBuffers(dev, &cai, &cb) != VK_SUCCESS ||
                CreateFence(dev, &fci, nullptr, &fence) != VK_SUCCESS) {
                ok = false; detailOut = "self-contained: cb/fence alloc failed";
            }
            if (ok) {
                VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                ib.image            = img;
                ib.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
                ib.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
                ib.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                ib.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                VkCommandBufferBeginInfo bi{
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                VkClearColorValue color{};
                color.float32[0] = 0.08f; color.float32[1] = 0.72f;
                color.float32[2] = 0.70f; color.float32[3] = 1.0f;
                const VkImageSubresourceRange range{
                    VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

                if (BeginCommandBuffer(cb, &bi) != VK_SUCCESS) {
                    ok = false; detailOut = "self-contained: Begin failed";
                } else {
                    CmdPipelineBarrier(cb,
                                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                       0, nullptr, 0, nullptr, 1, &ib);
                    CmdClearColorImage(cb, img,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       &color, 1, &range);
                    EndCommandBuffer(cb);
                    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                    si.commandBufferCount = 1;
                    si.pCommandBuffers    = &cb;
                    // OUR OWN queue on OUR OWN drm context — the submit
                    // this proof exists to validate. No shared state.
                    if (QueueSubmit(queue, 1, &si, fence) != VK_SUCCESS) {
                        ok = false; detailOut = "self-contained: QueueSubmit failed";
                    } else if (WaitForFences(dev, 1, &fence, VK_TRUE,
                                             3000000000ull) != VK_SUCCESS) {
                        ok = false; detailOut = "self-contained: fence timeout (3 s)";
                    } else {
                        detailOut = "fresh instance/device clear submit + fenced OK";
                    }
                }
            }
        }
        if (fence != VK_NULL_HANDLE) DestroyFence(dev, fence, nullptr);
        if (pool != VK_NULL_HANDLE) DestroyCommandPool(dev, pool, nullptr);
        if (mem  != VK_NULL_HANDLE) FreeMemory(dev, mem, nullptr);
        if (img  != VK_NULL_HANDLE) DestroyImage(dev, img, nullptr);
    }

    // Full teardown of everything THIS call created — the child then
    // exits; the parent's stack is untouched by construction.
    DestroyDevice(dev, nullptr);
    auto pfnDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        gipa(inst, "vkDestroyInstance"));
    if (pfnDestroyInstance) pfnDestroyInstance(inst, nullptr);
    Breadcrumb::Set("gpu.proof: done ok=%d", ok ? 1 : 0);

    char head[192];
    snprintf(head, sizeof(head),
             "self-contained proof on '%s' drv=%s vendor=0x%x dev=0x%x: ",
             props.deviceName, VkVersion(props.driverVersion).c_str(),
             props.vendorID, props.deviceID);
    detailOut = std::string(head) + (ok ? "PASS — " : "FAIL — ") + detailOut;
    PX5_LOGI(LogCategory::GPU, "Self-contained GPU proof: %s",
             detailOut.c_str());
    return ok;
}

// ---------------------------------------------------------------------------
// M7 gate proof (docs/milestones.md): self-contained readback chain with
// expected pixels. Construction is the PROVEN self-contained one — fresh
// instance -> fresh device -> own drm render node fd, full teardown — with
// the M7 chain on top:
//
//   synthetic IR list {Clear(red), Barrier}   (labelled: the M6 lowering
//      emits no Clear yet — this proves the BACKEND, not the decoder)
//        -> PlanVulkanCommands  (gpu/vulkan_backend.cpp — the planner the
//           M7-T1 host gate locks byte-for-byte)
//        -> image (TRANSFER_DST|TRANSFER_SRC) + host-visible buffer
//        -> record plan commands -> submit -> fence
//        -> proof plumbing: TRANSFER_SRC transition + vkCmdCopyImageToBuffer
//        -> map -> VerifyClearReadback (exact bytes) -> SHA-256
//
// The expected bytes come from plan.clearRgba — ONE conversion rule shared
// with the host gate, so host and device cannot drift apart. The detail
// line always carries the measured capability set (device/driver/api),
// never a required one (AGENTS.md rule 3).
// ---------------------------------------------------------------------------
bool VulkanGpuDevice::RunM7ClearReadbackProof(std::string& detailOut) {
    // PX5::Gpu names (the backend layer this proof drives).
    using Gpu::GpuOp;
    using Gpu::GpuOpList;
    using Gpu::OpKind;
    using Gpu::PlanVulkanCommands;
    using Gpu::ReadbackCheck;
    using Gpu::VerifyClearReadback;
    using Gpu::VulkanCommand;
    using Gpu::VulkanCommandPlan;

    void* lib = m_vulkanLib ? m_vulkanLib
                            : dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        detailOut = std::string("dlopen libvulkan.so failed: ") + dlerror();
        return false;
    }

    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(lib, "vkGetInstanceProcAddr"));
    if (!gipa) { detailOut = "vkGetInstanceProcAddr missing"; return false; }

    Breadcrumb::Set("gpu.m7: enter (self-contained)");

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "PX5-M7";
    appInfo.apiVersion       = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &appInfo;          // offscreen: no surface exts
    VkInstance inst = VK_NULL_HANDLE;
    auto pfnCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        gipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!pfnCreateInstance ||
        pfnCreateInstance(&ici, nullptr, &inst) != VK_SUCCESS || !inst) {
        detailOut = "m7: vkCreateInstance failed";
        return false;
    }
    Breadcrumb::Set("gpu.m7: instance ready");

    auto pfnEnumDevs = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        gipa(inst, "vkEnumeratePhysicalDevices"));
    auto pfnProps = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        gipa(inst, "vkGetPhysicalDeviceProperties"));
    auto pfnFams = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        gipa(inst, "vkGetPhysicalDeviceQueueFamilyProperties"));
    auto pfnMemProps = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        gipa(inst, "vkGetPhysicalDeviceMemoryProperties"));
    auto pfnCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        gipa(inst, "vkCreateDevice"));
    if (!pfnEnumDevs || !pfnProps || !pfnFams || !pfnMemProps ||
        !pfnCreateDevice) {
        detailOut = "m7: instance-level fns missing";
        return false;
    }

    uint32_t nd = 0;
    if (pfnEnumDevs(inst, &nd, nullptr) != VK_SUCCESS || nd == 0) {
        detailOut = "m7: no physical devices";
        return false;
    }
    std::vector<VkPhysicalDevice> devs(nd);
    pfnEnumDevs(inst, &nd, devs.data());
    VkPhysicalDevice pd = devs[0];
    VkPhysicalDeviceProperties props{};
    pfnProps(pd, &props);
    Breadcrumb::Set("gpu.m7: physical device ready");

    uint32_t nf = 0;
    pfnFams(pd, &nf, nullptr);
    if (nf == 0) { detailOut = "m7: no queue families"; return false; }
    std::vector<VkQueueFamilyProperties> fams(nf);
    pfnFams(pd, &nf, fams.data());
    uint32_t gfx = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < nf; ++i)
        if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { gfx = i; break; }
    if (gfx == 0xFFFFFFFFu) {
        detailOut = "m7: no graphics queue family";
        return false;
    }

    // Fresh extension-less logical device — offscreen-only, same contract
    // as RunSelfContainedProof.
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = gfx;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos    = &qci;
    VkDevice dev = VK_NULL_HANDLE;
    if (pfnCreateDevice(pd, &dci, nullptr, &dev) != VK_SUCCESS || !dev) {
        detailOut = "m7: vkCreateDevice failed";
        return false;
    }
    Breadcrumb::Set("gpu.m7: fresh device ready");

    auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        gipa(inst, "vkGetDeviceProcAddr"));
#define PX5_P(fn, name) \
    auto fn = reinterpret_cast<PFN_vk##fn>(gdpa ? gdpa(dev, name) : nullptr)
    PX5_P(GetDeviceQueue,         "vkGetDeviceQueue");
    PX5_P(CreateImage,            "vkCreateImage");
    PX5_P(AllocateMemory,         "vkAllocateMemory");
    PX5_P(BindImageMemory,        "vkBindImageMemory");
    PX5_P(CreateBuffer,           "vkCreateBuffer");
    PX5_P(BindBufferMemory,       "vkBindBufferMemory");
    PX5_P(CreateCommandPool,      "vkCreateCommandPool");
    PX5_P(AllocateCommandBuffers, "vkAllocateCommandBuffers");
    PX5_P(BeginCommandBuffer,     "vkBeginCommandBuffer");
    PX5_P(EndCommandBuffer,       "vkEndCommandBuffer");
    PX5_P(CmdPipelineBarrier,     "vkCmdPipelineBarrier");
    PX5_P(CmdClearColorImage,     "vkCmdClearColorImage");
    PX5_P(CmdCopyImageToBuffer,   "vkCmdCopyImageToBuffer");
    PX5_P(QueueSubmit,            "vkQueueSubmit");
    PX5_P(WaitForFences,          "vkWaitForFences");
    PX5_P(CreateFence,            "vkCreateFence");
    PX5_P(DestroyFence,           "vkDestroyFence");
    PX5_P(DestroyCommandPool,     "vkDestroyCommandPool");
    PX5_P(FreeMemory,             "vkFreeMemory");
    PX5_P(DestroyImage,           "vkDestroyImage");
    PX5_P(DestroyBuffer,          "vkDestroyBuffer");
    PX5_P(MapMemory,              "vkMapMemory");
    PX5_P(UnmapMemory,            "vkUnmapMemory");
    PX5_P(DestroyDevice,          "vkDestroyDevice");
#undef PX5_P
    auto GetImageMemReqs = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
        gdpa ? gdpa(dev, "vkGetImageMemoryRequirements") : nullptr);
    auto GetBufferMemReqs = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
        gdpa ? gdpa(dev, "vkGetBufferMemoryRequirements") : nullptr);

    bool ok = false;
    std::string err;
    char m7Sha[65] = {0};   // readback buffer hash, printed on PASS
    if (!GetDeviceQueue || !CreateImage || !GetImageMemReqs ||
        !AllocateMemory || !BindImageMemory || !CreateBuffer ||
        !GetBufferMemReqs || !BindBufferMemory || !CreateCommandPool ||
        !AllocateCommandBuffers || !BeginCommandBuffer || !EndCommandBuffer ||
        !CmdPipelineBarrier || !CmdClearColorImage || !CmdCopyImageToBuffer ||
        !QueueSubmit || !WaitForFences || !CreateFence || !DestroyFence ||
        !DestroyCommandPool || !FreeMemory || !DestroyImage ||
        !DestroyBuffer || !MapMemory || !UnmapMemory || !DestroyDevice) {
        err = "device-level fns missing";
    } else {
        // --- The IR input: one Clear + the submit boundary. Labelled ---
        // synthetic IN THE LOG: the M6 lowering emits no Clear op yet, so
        // this list is built by hand to prove the BACKEND chain. It is a
        // real GpuOpList and the planner below is the real production one.
        GpuOpList irOps;
        {
            GpuOp clear{};
            clear.kind          = OpKind::kClear;
            clear.seq           = 1;
            clear.clearColor[0] = 1.0f;   // clear = red, docs/testing.md M7
            clear.clearColor[1] = 0.0f;
            clear.clearColor[2] = 0.0f;
            clear.clearColor[3] = 1.0f;
            irOps.Push(clear);
            GpuOp barrier{};
            barrier.kind         = OpKind::kBarrier;
            barrier.seq          = 1;
            barrier.barrierScope = 0;
            irOps.Push(barrier);
        }
        Breadcrumb::Set("gpu.m7: ir list built ops=%zu", irOps.Size());

        const VulkanCommandPlan plan = PlanVulkanCommands(irOps);
        Breadcrumb::Set("gpu.m7: plan %s", plan.stats.SummaryString().c_str());
        if (!plan.hasClear) {
            err = "plan has no clear (backend contract violated)";
        } else {
            VkQueue queue = VK_NULL_HANDLE;
            GetDeviceQueue(dev, gfx, 0, &queue);
            ok = queue != VK_NULL_HANDLE;
            err = ok ? "" : "GetDeviceQueue null";

            constexpr uint32_t W = 64, H = 64;
            constexpr VkDeviceSize kBufBytes =
                static_cast<VkDeviceSize>(W) * H * 4;

            // Target image: clear (TRANSFER_DST) + readback copy
            // (TRANSFER_SRC) — one more usage bit than the v1.45 proof.
            VkImage img = VK_NULL_HANDLE;
            if (ok) {
                VkImageCreateInfo ici2{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
                ici2.imageType     = VK_IMAGE_TYPE_2D;
                ici2.format        = VK_FORMAT_R8G8B8A8_UNORM;
                ici2.extent        = {W, H, 1};
                ici2.mipLevels     = 1;
                ici2.arrayLayers   = 1;
                ici2.samples       = VK_SAMPLE_COUNT_1_BIT;
                ici2.tiling        = VK_IMAGE_TILING_OPTIMAL;
                ici2.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                ici2.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                if (CreateImage(dev, &ici2, nullptr, &img) != VK_SUCCESS) {
                    ok = false; err = "CreateImage failed";
                }
            }

            VkDeviceMemory imgMem = VK_NULL_HANDLE;
            if (ok) {
                VkMemoryRequirements reqs{};
                GetImageMemReqs(dev, img, &reqs);
                VkPhysicalDeviceMemoryProperties mp{};
                pfnMemProps(pd, &mp);
                VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                mai.allocationSize  = reqs.size;
                mai.memoryTypeIndex =
                    SelectImageMemoryType(mp, reqs.memoryTypeBits);
                if (AllocateMemory(dev, &mai, nullptr, &imgMem) != VK_SUCCESS ||
                    BindImageMemory(dev, img, imgMem, 0) != VK_SUCCESS) {
                    ok = false; err = "image memory alloc/bind failed";
                }
            }

            // Readback buffer: HOST_VISIBLE|HOST_COHERENT is a capability
            // requirement (CPU must map it), not a preference — so it does
            // NOT go through the vramUsageMode-aware image selector.
            VkBuffer buf = VK_NULL_HANDLE;
            VkDeviceMemory bufMem = VK_NULL_HANDLE;
            if (ok) {
                VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
                bci.size  = kBufBytes;
                bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                if (CreateBuffer(dev, &bci, nullptr, &buf) != VK_SUCCESS) {
                    ok = false; err = "CreateBuffer failed";
                } else {
                    VkMemoryRequirements reqs{};
                    GetBufferMemReqs(dev, buf, &reqs);
                    VkPhysicalDeviceMemoryProperties mp{};
                    pfnMemProps(pd, &mp);
                    int32_t hostType = -1;
                    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
                        const VkMemoryType t = mp.memoryTypes[i];
                        if ((reqs.memoryTypeBits & (1u << i)) &&
                            (t.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                            (t.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                            hostType = static_cast<int32_t>(i);
                            break;
                        }
                    }
                    if (hostType < 0) {
                        ok = false; err = "no HOST_VISIBLE|COHERENT memory type";
                    } else {
                        VkMemoryAllocateInfo mai{
                            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                        mai.allocationSize  = reqs.size;
                        mai.memoryTypeIndex = static_cast<uint32_t>(hostType);
                        if (AllocateMemory(dev, &mai, nullptr, &bufMem)
                                != VK_SUCCESS ||
                            BindBufferMemory(dev, buf, bufMem, 0)
                                != VK_SUCCESS) {
                            ok = false; err = "buffer memory alloc/bind failed";
                        }
                    }
                }
            }

            VkCommandPool pool = VK_NULL_HANDLE;
            if (ok) {
                VkCommandPoolCreateInfo pci{
                    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
                pci.flags            =
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                pci.queueFamilyIndex = gfx;
                if (CreateCommandPool(dev, &pci, nullptr, &pool)
                        != VK_SUCCESS) {
                    ok = false; err = "CreateCommandPool failed";
                }
            }

            VkCommandBuffer cbs[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
            VkFence fences[2]      = {VK_NULL_HANDLE, VK_NULL_HANDLE};
            if (ok) {
                VkCommandBufferAllocateInfo cai{
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                cai.commandPool        = pool;
                cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                cai.commandBufferCount = 2;
                VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                if (AllocateCommandBuffers(dev, &cai, cbs) != VK_SUCCESS ||
                    CreateFence(dev, &fci, nullptr, &fences[0]) != VK_SUCCESS ||
                    CreateFence(dev, &fci, nullptr, &fences[1]) != VK_SUCCESS) {
                    ok = false; err = "cb/fence alloc failed";
                }
            }

            // --- Submission 1: the PLANNED commands ------------------------
            if (ok) {
                VkCommandBufferBeginInfo bi{
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                Breadcrumb::Set("gpu.m7: record plan (%zu cmds)",
                                plan.commands.size());
                bool recorded = BeginCommandBuffer(cbs[0], &bi) == VK_SUCCESS;
                if (recorded) {
                    for (const VulkanCommand& c : plan.commands) {
                        switch (c.kind) {
                        case VulkanCommand::Kind::kPipelineBarrier: {
                            VkImageMemoryBarrier ib{
                                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                            ib.image         = img;
                            ib.srcAccessMask = 0;
                            ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                            ib.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
                            ib.newLayout     =
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                            ib.subresourceRange = {
                                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                            CmdPipelineBarrier(cbs[0],
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                0, nullptr, 0, nullptr, 1, &ib);
                            break;
                        }
                        case VulkanCommand::Kind::kClearColorImage: {
                            VkClearColorValue color{};
                            for (int i = 0; i < 4; ++i)
                                color.float32[i] = c.clearColor[i];
                            const VkImageSubresourceRange range{
                                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                            CmdClearColorImage(cbs[0], img,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                &color, 1, &range);
                            break;
                        }
                        case VulkanCommand::Kind::kSubmitBoundary:
                            // The IR Barrier op materializes here: end ->
                            // submit -> fence. Commands after a boundary
                            // would need a new command buffer; the M7 plan
                            // ends at the first one.
                            recorded = EndCommandBuffer(cbs[0]) == VK_SUCCESS;
                            break;
                        }
                        if (!recorded) break;
                    }
                }
                if (!recorded) {
                    ok = false; err = "plan recording failed";
                } else {
                    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                    si.commandBufferCount = 1;
                    si.pCommandBuffers    = &cbs[0];
                    Breadcrumb::Set("gpu.m7: submit1");
                    if (QueueSubmit(queue, 1, &si, fences[0]) != VK_SUCCESS) {
                        ok = false; err = "QueueSubmit (plan) failed";
                    } else if (WaitForFences(dev, 1, &fences[0], VK_TRUE,
                                             3000000000ull) != VK_SUCCESS) {
                        ok = false; err = "fence timeout after clear (3 s)";
                    }
                }
            }

            // --- Submission 2: READBACK PLUMBING (not IR semantics — the IR
            // has no readback op; the proof owns the copy) -----------------
            ReadbackCheck rb{};
            if (ok) {
                VkCommandBufferBeginInfo bi{
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                if (BeginCommandBuffer(cbs[1], &bi) != VK_SUCCESS) {
                    ok = false; err = "Begin (readback) failed";
                } else {
                    VkImageMemoryBarrier ib{
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    ib.image         = img;
                    ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    ib.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    ib.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    ib.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    ib.subresourceRange = {
                        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                    CmdPipelineBarrier(cbs[1],
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                        0, nullptr, 0, nullptr, 1, &ib);

                    VkBufferImageCopy region{};
                    region.bufferOffset      = 0;
                    region.bufferRowLength   = 0;   // tightly packed
                    region.bufferImageHeight = 0;
                    region.imageSubresource = {
                        VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                    region.imageOffset       = {0, 0, 0};
                    region.imageExtent       = {W, H, 1};
                    CmdCopyImageToBuffer(cbs[1], img,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1,
                        &region);
                    EndCommandBuffer(cbs[1]);

                    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                    si.commandBufferCount = 1;
                    si.pCommandBuffers    = &cbs[1];
                    Breadcrumb::Set("gpu.m7: readback submit");
                    if (QueueSubmit(queue, 1, &si, fences[1]) != VK_SUCCESS) {
                        ok = false; err = "QueueSubmit (readback) failed";
                    } else if (WaitForFences(dev, 1, &fences[1], VK_TRUE,
                                             3000000000ull) != VK_SUCCESS) {
                        ok = false; err = "fence timeout after copy (3 s)";
                    }
                }
            }

            // --- Map + verify + hash ----------------------------------------
            if (ok) {
                void* mapped = nullptr;
                if (MapMemory(dev, bufMem, 0, VK_WHOLE_SIZE, 0, &mapped)
                        != VK_SUCCESS || mapped == nullptr) {
                    ok = false; err = "vkMapMemory failed";
                } else {
                    rb = VerifyClearReadback(static_cast<const uint8_t*>(mapped),
                                             static_cast<size_t>(kBufBytes),
                                             W, H, plan.clearRgba);
                    char sha[65] = {0};
                    Evidence::Sha256Hex(mapped,
                                        static_cast<size_t>(kBufBytes), sha);
                    memcpy(m7Sha, sha, sizeof(m7Sha));
                    UnmapMemory(dev, bufMem);
                    Breadcrumb::Set("gpu.m7: verify %u/%u",
                                    rb.pixelsMatch, rb.pixelsTotal);
                    if (!rb.allMatch) {
                        char det[128];
                        snprintf(det, sizeof(det),
                                 "pixel mismatch: %u/%u match, first bad byte %zu",
                                 rb.pixelsMatch, rb.pixelsTotal,
                                 rb.firstBadByte);
                        ok = false; err = det;
                    }
                }
            }

            if (fences[0] != VK_NULL_HANDLE) DestroyFence(dev, fences[0], nullptr);
            if (fences[1] != VK_NULL_HANDLE) DestroyFence(dev, fences[1], nullptr);
            if (pool  != VK_NULL_HANDLE) DestroyCommandPool(dev, pool, nullptr);
            if (bufMem != VK_NULL_HANDLE) FreeMemory(dev, bufMem, nullptr);
            if (buf   != VK_NULL_HANDLE) DestroyBuffer(dev, buf, nullptr);
            if (imgMem != VK_NULL_HANDLE) FreeMemory(dev, imgMem, nullptr);
            if (img   != VK_NULL_HANDLE) DestroyImage(dev, img, nullptr);

            if (ok) {
                char det[160];
                snprintf(det, sizeof(det),
                         "IR Clear readback: pixels %u/%u match "
                         "(%u,%u,%u,%u) | plan: %s",
                         rb.pixelsMatch, rb.pixelsTotal,
                         plan.clearRgba[0], plan.clearRgba[1],
                         plan.clearRgba[2], plan.clearRgba[3],
                         plan.stats.SummaryString().c_str());
                err = det;
            }
        }
    }

    // Full teardown of everything THIS call created — same contract as
    // RunSelfContainedProof: the child (if any) exits; the parent's stack
    // is untouched by construction.
    DestroyDevice(dev, nullptr);
    auto pfnDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        gipa(inst, "vkDestroyInstance"));
    if (pfnDestroyInstance) pfnDestroyInstance(inst, nullptr);
    Breadcrumb::Set("gpu.m7: done ok=%d", ok ? 1 : 0);

    char head[224];
    snprintf(head, sizeof(head),
             "self-contained M7 readback on '%s' drv=%s api=%s "
             "vendor=0x%x dev=0x%x: ",
             props.deviceName, VkVersion(props.driverVersion).c_str(),
             VkVersion(props.apiVersion).c_str(), props.vendorID,
             props.deviceID);
    detailOut = std::string(head) + (ok ? "PASS — " : "FAIL — ") + err;
    if (ok && m7Sha[0] != '\0') {
        detailOut += " | sha256=";
        detailOut += m7Sha;
    }
    PX5_LOGI(LogCategory::GPU, "M7 readback proof: %s", detailOut.c_str());
    return ok;
}

// ---------------------------------------------------------------------------
// On-screen surface + swapchain
// ---------------------------------------------------------------------------
bool VulkanGpuDevice::AttachWindowSurface(ANativeWindow* window) {
    if (!window) return false;
    if (!EnsureLogicalDevice()) return false;

    auto vkCreateAndroidSurface =
        reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
            dlsym(m_vulkanLib, "vkCreateAndroidSurfaceKHR"));
    if (!vkCreateAndroidSurface) {
        m_stats.SetError("vkCreateAndroidSurfaceKHR missing");
        return false;
    }

    std::lock_guard<std::mutex> lk(m_gpuMutex);

    // One strong reference held by the device until Detach/Shutdown.
    ANativeWindow_acquire(window);
    m_window = window;

    VkAndroidSurfaceCreateInfoKHR sci{
        VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    sci.window = m_window;
    if (vkCreateAndroidSurface(reinterpret_cast<VkInstance>(m_instance),
                               &sci, nullptr, &m_surface) != VK_SUCCESS ||
        !m_surface) {
        m_stats.SetError("surface creation failed");
        ANativeWindow_release(m_window);
        m_window = nullptr;
        return false;
    }

    m_stats.surfaceActive.store(true);
    PX5_LOGI(LogCategory::GPU, "ANativeWindow surface attached (%dx%d)",
             ANativeWindow_getWidth(m_window),
             ANativeWindow_getHeight(m_window));
    return true;
}

void VulkanGpuDevice::DetachWindowSurface() {
    StopRenderLoop();
    std::lock_guard<std::mutex> lk(m_gpuMutex);
    DestroySwapchainLocked();
    if (m_window) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }
    if (m_surface != VK_NULL_HANDLE) {
        auto vkDestroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            dlsym(m_vulkanLib, "vkDestroySurfaceKHR"));
        if (vkDestroySurface)
            vkDestroySurface(reinterpret_cast<VkInstance>(m_instance),
                             m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_stats.surfaceActive.store(false);
}

bool VulkanGpuDevice::ChooseExtent(uint32_t& w, uint32_t& h) const {
    if (!m_window) return false;
    const int32_t nw = ANativeWindow_getWidth(m_window);
    const int32_t nh = ANativeWindow_getHeight(m_window);
    if (nw <= 0 || nh <= 0) return false;

    const int pct = EngineSettings::resScalePct.load();
    w = static_cast<uint32_t>((static_cast<int64_t>(nw) * pct) / 100);
    h = static_cast<uint32_t>((static_cast<int64_t>(nh) * pct) / 100);

    auto pfnCaps =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            dlsym(m_vulkanLib,
                  "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (pfnCaps) {
        VkSurfaceCapabilitiesKHR caps{};
        pfnCaps(reinterpret_cast<VkPhysicalDevice>(m_physDev),
                m_surface, &caps);
        if (caps.currentExtent.width != 0xFFFFFFFFu) {
            const bool scaledOk =
                caps.minImageExtent.width  <= w &&
                w <= caps.maxImageExtent.width &&
                caps.minImageExtent.height <= h &&
                h <= caps.maxImageExtent.height;
            if (!scaledOk) {                     // most Android drivers
                w = caps.currentExtent.width;
                h = caps.currentExtent.height;
            }
        } else {
            w = std::clamp(w, caps.minImageExtent.width,
                              caps.maxImageExtent.width);
            h = std::clamp(h, caps.minImageExtent.height,
                              caps.maxImageExtent.height);
        }
    }
    return true;
}

bool VulkanGpuDevice::CreateSwapchainLocked() {
    if (!m_swapchainExtAvailable) {
        // Honest refusal instead of a null-trampoline call: the extension
        // was not enabled at device creation (or the device lacks it), so
        // the swapchain entry points are not wired. See
        // EnsureLogicalDeviceUnlocked / BuildDeviceTable (v1.18).
        m_stats.SetError("swapchain: VK_KHR_swapchain not enabled on device");
        return false;
    }
    if (!m_surface || !m_device) {
        m_stats.SetError("swapchain: no surface/device");
        return false;
    }

    uint32_t w = 0, h = 0;
    if (!ChooseExtent(w, h)) { m_stats.SetError("extent: window gone"); return false; }

    auto pfnFormats =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            dlsym(m_vulkanLib, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    uint32_t nf = 0;
    pfnFormats(reinterpret_cast<VkPhysicalDevice>(m_physDev), m_surface,
               &nf, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(nf ? nf : 1);
    if (nf)
        pfnFormats(reinterpret_cast<VkPhysicalDevice>(m_physDev),
                   m_surface, &nf, formats.data());

    VkFormat wantFormat = VK_FORMAT_R8G8B8A8_UNORM;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM) { wantFormat = f.format; break; }
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM) wantFormat = f.format;
    }
    m_swapFormat = wantFormat;

    auto pfnPMs =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
            dlsym(m_vulkanLib,
                  "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    uint32_t npm = 0;
    pfnPMs(reinterpret_cast<VkPhysicalDevice>(m_physDev), m_surface,
           &npm, nullptr);
    std::vector<VkPresentModeKHR> pms(npm ? npm : 1);
    if (npm)
        pfnPMs(reinterpret_cast<VkPhysicalDevice>(m_physDev), m_surface,
               &npm, pms.data());

    bool haveFifo = false, haveMailbox = false, haveImmediate = false,
         haveFifoRelaxed = false, haveFifoLatestReady = false;
    for (auto m : pms) {
        if (m == VK_PRESENT_MODE_FIFO_KHR)            haveFifo = true;
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)         haveMailbox = true;
        if (m == VK_PRESENT_MODE_IMMEDIATE_KHR)       haveImmediate = true;
        if (m == VK_PRESENT_MODE_FIFO_RELAXED_KHR)    haveFifoRelaxed = true;
#ifdef VK_PRESENT_MODE_FIFO_LATEST_READY_EXT
        if (m == VK_PRESENT_MODE_FIFO_LATEST_READY_EXT) haveFifoLatestReady = true;
#endif
    }

    // Selection order: an explicit user choice wins ONLY when the device
    // reports support for it (queried right above — never forced); otherwise
    // the auto policy applies and the fallback is logged, never silent.
    VkPresentModeKHR chosen = VK_PRESENT_MODE_FIFO_KHR;
    std::string pmName = "FIFO(vsync)";
    const int wantMode = EngineSettings::presentMode.load();
    bool honored = false;
    switch (wantMode) {
        case 1: if (haveFifo)              { chosen = VK_PRESENT_MODE_FIFO_KHR;         pmName = "FIFO";           honored = true; } break;
        case 2: if (haveFifoRelaxed)       { chosen = VK_PRESENT_MODE_FIFO_RELAXED_KHR; pmName = "FIFO_RELAXED";   honored = true; } break;
        case 3: if (haveMailbox)           { chosen = VK_PRESENT_MODE_MAILBOX_KHR;      pmName = "MAILBOX";        honored = true; } break;
        case 4: if (haveImmediate)         { chosen = VK_PRESENT_MODE_IMMEDIATE_KHR;    pmName = "IMMEDIATE";      honored = true; } break;
#ifdef VK_PRESENT_MODE_FIFO_LATEST_READY_EXT
        case 5: if (haveFifoLatestReady)   { chosen = VK_PRESENT_MODE_FIFO_LATEST_READY_EXT; pmName = "FIFO_LATEST_READY"; honored = true; } break;
#endif
        default: break; // 0 = auto
    }
    if (wantMode > 0 && !honored) {
        PX5_LOGW(LogCategory::VULKAN,
                 "present mode %d requested but not supported by this device/"
                 "surface — falling back (supported: FIFO=%d MAILBOX=%d "
                 "IMMEDIATE=%d FIFO_RELAXED=%d LATEST_READY=%d)",
                 wantMode, haveFifo ? 1 : 0, haveMailbox ? 1 : 0,
                 haveImmediate ? 1 : 0, haveFifoRelaxed ? 1 : 0,
                 haveFifoLatestReady ? 1 : 0);
    }
    if (!honored) {
        pmName = "FIFO(vsync)";
        if (!EngineSettings::vsyncEnabled.load()) {
            if (haveMailbox) {
                chosen = VK_PRESENT_MODE_MAILBOX_KHR;
                pmName = "MAILBOX";
            } else if (haveImmediate) {
                chosen = VK_PRESENT_MODE_IMMEDIATE_KHR;
                pmName = "IMMEDIATE";
            }
        }
    }
    if (chosen == VK_PRESENT_MODE_FIFO_KHR && !haveFifo && !pms.empty())
        chosen = pms[0];

    {
        std::lock_guard<std::mutex> sl(m_stats.lock);
        m_stats.presentMode  = pmName;
        m_stats.width        = w;
        m_stats.height       = h;
    }

    VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sc.surface          = m_surface;
    sc.minImageCount    = 3;
    sc.imageFormat      = m_swapFormat;
    sc.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sc.imageExtent      = {w, h};
    sc.imageArrayLayers = 1;
    sc.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sc.preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sc.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sc.presentMode      = chosen;
    sc.clipped          = VK_TRUE;

    if (m_tbl.CreateSwapchain(m_device, &sc, nullptr, &m_swapchain)
            != VK_SUCCESS || !m_swapchain) {
        m_stats.SetError("CreateSwapchain failed");
        return false;
    }

    PX5_LOGI(LogCategory::GPU,
             "Swapchain ready: %ux%u fmt=%d present=%s",
             w, h, static_cast<int>(m_swapFormat), pmName.c_str());
    return true;
}

void VulkanGpuDevice::DestroySwapchainLocked() {
    if (m_swapchain != VK_NULL_HANDLE && m_tbl.DestroySwapchain) {
        if (m_queue != VK_NULL_HANDLE && m_tbl.QueueWaitIdle)
            m_tbl.QueueWaitIdle(m_queue);
        m_tbl.DestroySwapchain(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// Render loop
// ---------------------------------------------------------------------------
bool VulkanGpuDevice::StartRenderLoop() {
    if (!m_surface) {
        m_stats.SetError("render: no surface attached");
        return false;
    }
    if (m_rendering.load()) return true;
    if (!EnsureLogicalDevice()) return false;

    m_rendering.store(true);
    m_stats.SetError("");

    m_renderThread = std::thread([this] {
        if (!CreateSwapchainLocked()) {
            m_rendering.store(false);
            return;
        }

        VkCommandPoolCreateInfo pci{
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.flags            =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = m_queueFamily;
        VkCommandPool pool = VK_NULL_HANDLE;
        // v1.15: render-loop resource creation results were ignored; a
        // silent failure would later submit through garbage handles.
        if (m_tbl.CreateCommandPool(m_device, &pci, nullptr, &pool)
                    != VK_SUCCESS || pool == VK_NULL_HANDLE) {
            m_stats.SetError("render: CreateCommandPool failed");
            m_rendering.store(false);
            return;
        }

        VkCommandBufferAllocateInfo cai{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool        = pool;
        cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (m_tbl.AllocateCommandBuffers(m_device, &cai, &cb)
                    != VK_SUCCESS || cb == VK_NULL_HANDLE) {
            m_stats.SetError("render: AllocateCommandBuffers failed");
            m_rendering.store(false);
            return;
        }

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore acquireSem = VK_NULL_HANDLE;
        VkSemaphore presentSem = VK_NULL_HANDLE;
        if (m_tbl.CreateSemaphore(m_device, &sci, nullptr, &acquireSem)
                    != VK_SUCCESS ||
            m_tbl.CreateSemaphore(m_device, &sci, nullptr, &presentSem)
                    != VK_SUCCESS ||
            acquireSem == VK_NULL_HANDLE || presentSem == VK_NULL_HANDLE) {
            m_stats.SetError("render: CreateSemaphore failed");
            m_rendering.store(false);
            return;
        }

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence inflight = VK_NULL_HANDLE;
        if (m_tbl.CreateFence(m_device, &fci, nullptr, &inflight)
                    != VK_SUCCESS || inflight == VK_NULL_HANDLE) {
            m_stats.SetError("render: CreateFence failed");
            m_rendering.store(false);
            return;
        }

        uint64_t frameCounter = 0;
        int      failStreak   = 0;

        while (m_rendering.load()) {
            uint32_t imgIdx = 0;
            const VkResult ar = m_tbl.AcquireNextImage(
                m_device, m_swapchain, 100000000ull /*100 ms*/,
                acquireSem, VK_NULL_HANDLE, &imgIdx);

            if (ar == VK_TIMEOUT || ar == VK_NOT_READY) {
                m_stats.acquireTimeouts.fetch_add(1);
                continue;
            }
            if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) {
                // OUT_OF_DATE / surface loss: honest rebuild.
                m_stats.recreations.fetch_add(1);
                if (++failStreak > 3) {
                    m_stats.SetError("acquire kept failing");
                    break;
                }
                std::lock_guard<std::mutex> lk(m_gpuMutex);
                DestroySwapchainLocked();
                if (!CreateSwapchainLocked()) break;
                continue;
            }
            failStreak = 0;

            m_tbl.WaitForFences(m_device, 1, &inflight, VK_TRUE,
                                3000000000ull);
            m_tbl.ResetFences(m_device, 1, &inflight);

            // Subtle PX5 navy breathing between two frames of the pair.
            VkClearColorValue color{};
            color.float32[0] = frameCounter % 2 ? 0.055f : 0.031f;
            color.float32[1] = 0.094f;
            color.float32[2] = 0.160f;
            color.float32[3] = 1.0f;

            if (!RecordAndSubmitFrame(imgIdx, acquireSem, presentSem,
                                      color, cb, inflight)) {
                if (++failStreak > 3) { m_stats.SetError("submit failed"); break; }
                continue;
            }
            frameCounter++;
            m_stats.frames.store(frameCounter);
        }

        if (pool != VK_NULL_HANDLE)
            m_tbl.DestroyCommandPool(m_device, pool, nullptr);
        if (acquireSem != VK_NULL_HANDLE)
            m_tbl.DestroySemaphore(m_device, acquireSem, nullptr);
        if (presentSem != VK_NULL_HANDLE)
            m_tbl.DestroySemaphore(m_device, presentSem, nullptr);
        if (inflight != VK_NULL_HANDLE)
            m_tbl.DestroyFence(m_device, inflight, nullptr);

        {
            std::lock_guard<std::mutex> lk(m_gpuMutex);
            DestroySwapchainLocked();
        }
        m_rendering.store(false);
    });

    return true;
}

bool VulkanGpuDevice::RecordAndSubmitFrame(uint32_t imageIndex,
                                           VkSemaphore acquireSem,
                                           VkSemaphore presentSem,
                                           VkClearColorValue color,
                                           VkCommandBuffer cb,
                                           VkFence inflight) {
    uint32_t got = 0;
    if (!m_tbl.GetSwapchainImages(m_device, m_swapchain, &got, nullptr) ||
        got == 0 || imageIndex >= got)
        return false;

    std::vector<VkImage> imgs(got);
    if (!m_tbl.GetSwapchainImages(m_device, m_swapchain, &got, imgs.data()))
        return false;
    const VkImage image = imgs[imageIndex];

    m_tbl.ResetCommandBuffer(cb, 0);

    VkCommandBufferBeginInfo bi{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkImageMemoryBarrier toDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toDst.image            = image;
    toDst.srcAccessMask    = 0;
    toDst.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
    toDst.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
    toDst.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageMemoryBarrier toPresent{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toPresent.image            = image;
    toPresent.srcAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
    toPresent.dstAccessMask    = 0;
    toPresent.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    const VkImageSubresourceRange range{
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (m_tbl.BeginCommandBuffer(cb, &bi) != VK_SUCCESS) return false;
    m_tbl.CmdPipelineBarrier(cb,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toDst);
    m_tbl.CmdClearColorImage(cb, image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &color, 1, &range);
    m_tbl.CmdPipelineBarrier(cb,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toPresent);
    if (m_tbl.EndCommandBuffer(cb) != VK_SUCCESS) return false;

    const VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &acquireSem;
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &presentSem;
    VkResult submitResult = VK_RESULT_MAX_ENUM;
    VkResult presentResult = VK_RESULT_MAX_ENUM;
    {
        // Queue external-sync contract (see m_queueMutex): the offscreen
        // proof may submit on the same m_queue concurrently. The missing
        // lock here is the 2026-08-30 three-times-identical SIGSEGV at
        // EmuScreen entry.
        std::lock_guard<std::mutex> qlk(m_queueMutex);
        submitResult = m_tbl.QueueSubmit(m_queue, 1, &si, inflight);
        if (submitResult != VK_SUCCESS) return false;

        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores    = &presentSem;
        pi.swapchainCount     = 1;
        pi.pSwapchains        = &m_swapchain;
        pi.pImageIndices      = &imageIndex;
        presentResult = m_tbl.QueuePresent(m_queue, &pi);
    }
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR) {
        m_stats.recreations.fetch_add(1);       // next acquire recreates
    } else if (presentResult != VK_SUCCESS) {
        return false;
    }
    return true;
}

std::string VulkanGpuDevice::GetRenderStatsString() const {
    const bool dev  = m_stats.deviceReady.load();
    const bool surf = m_stats.surfaceActive.load();

    std::lock_guard<std::mutex> sl(m_stats.lock);
    char buf[288];
    snprintf(buf, sizeof(buf),
             "GPU device: %s | surface: %s | frames=%llu (%ux%u) | "
             "present=%s | recreates=%u timeouts=%u%s%s",
             dev ? "ready" : "unavailable",
             surf ? (m_window ? "live" : "active") : "none",
             (unsigned long long)m_stats.frames.load(),
             m_stats.width, m_stats.height,
             m_stats.presentMode.c_str(),
             m_stats.recreations.load(), m_stats.acquireTimeouts.load(),
             m_stats.error.empty() ? "" : " | err=",
             m_stats.error.c_str());
    return buf;
}

void VulkanGpuDevice::StopRenderLoop() {
    const bool wasRunning = m_rendering.exchange(false);
    if (m_renderThread.joinable()) {
        m_renderThread.join();
        if (wasRunning)
            PX5_LOGI(LogCategory::GPU, "Render loop stopped (frames=%llu)",
                     (unsigned long long)m_stats.frames.load());
    }
}

// v1.16 — GPU-proof containment window (see header comment). The join in
// StopRenderLoop guarantees the render thread is fully out of any submit
// before the probe child is forked; no mutex is held across fork (a
// lock inherited by the child would deadlock its own proof submit).
bool VulkanGpuDevice::StopRenderLoopForProbe() {
    const bool wasRunning = m_rendering.load();
    StopRenderLoop();
    return wasRunning;
}

void VulkanGpuDevice::ResumeRenderLoopAfterProbe() {
    if (!StartRenderLoop()) {
        // Honest: a failed resume is a visible defect, not silence.
        PX5_LOGE(LogCategory::GPU,
                 "Render loop resume after GPU proof failed — screen stays "
                 "static until surface re-attach");
    }
}

} // namespace PX5
