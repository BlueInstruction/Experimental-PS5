#ifndef PX5_VULKAN_DEVICE_H
#define PX5_VULKAN_DEVICE_H

#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>

#include <vulkan/vulkan_core.h>

struct ANativeWindow;

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

// ---------------------------------------------------------------------------
// VulkanGpuDevice v2 — runtime enumeration AND honest rendering evidence.
//
// Layer 1 (Phase A, kept): dlopen loader -> instance -> physical devices.
// Layer 2 (this phase): logical device + queue + REAL command submission
//   via an offscreen clear proof (no window required), plus an on-screen
//   ANativeWindow swapchain loop on a dedicated render thread whose
//   present-mode honours EngineSettings::vsyncEnabled and whose extent
//   honours EngineSettings::resScalePct.
//
// Nothing is faked: every failure records its exact stage in
// capabilities.lastError / stats error string and returns false.
// ---------------------------------------------------------------------------
class VulkanGpuDevice {
public:
    static VulkanGpuDevice& GetInstance();

    // ---- Layer 1 ---------------------------------------------------------
    bool Initialize();       // loader -> instance -> physical devices
    void Shutdown();

    const GpuCapabilities& GetCapabilities() const { return m_caps; }
    std::string GetSummaryString() const;

    // ---- Layer 2 ---------------------------------------------------------
    bool EnsureLogicalDevice();                      // idempotent

    // Headless proof: image alloc + clear-image submit + fence wait.
    bool RunOffscreenClearProof(std::string& detailOut);

    // On-screen path driven by the emu screen SurfaceView lifecycle.
    bool AttachWindowSurface(ANativeWindow* window);
    void DetachWindowSurface();
    bool StartRenderLoop();
    void StopRenderLoop();

    struct RenderStats {
        std::atomic<uint64_t> frames{0};
        std::atomic<uint32_t> recreations{0};
        std::atomic<uint32_t> acquireTimeouts{0};
        std::atomic<bool>     surfaceActive{false};
        std::atomic<bool>     deviceReady{false};
        uint32_t              width = 0;
        uint32_t              height = 0;
        std::string           presentMode;
        std::string           error;
        mutable std::mutex    lock;                 // guards strings/extent
        std::string           LastError() const;
        void                  SetError(const std::string& e);
    };
    RenderStats& Stats() { return m_stats; }

    std::string GetRenderStatsString() const;

private:
    VulkanGpuDevice() = default;
    ~VulkanGpuDevice() = default;

    struct DeviceTable {
        PFN_vkGetDeviceQueue              GetDeviceQueue             = nullptr;
        PFN_vkCreateCommandPool           CreateCommandPool          = nullptr;
        PFN_vkAllocateCommandBuffers      AllocateCommandBuffers     = nullptr;
        PFN_vkBeginCommandBuffer          BeginCommandBuffer         = nullptr;
        PFN_vkCmdClearColorImage          CmdClearColorImage         = nullptr;
        PFN_vkCmdPipelineBarrier          CmdPipelineBarrier         = nullptr;
        PFN_vkEndCommandBuffer            EndCommandBuffer           = nullptr;
        PFN_vkResetCommandBuffer          ResetCommandBuffer         = nullptr;
        PFN_vkCreateFence                 CreateFence                = nullptr;
        PFN_vkDestroyFence                DestroyFence               = nullptr;
        PFN_vkCreateSemaphore             CreateSemaphore            = nullptr;
        PFN_vkDestroySemaphore            DestroySemaphore           = nullptr;
        PFN_vkCreateImage                 CreateImage                = nullptr;
        PFN_vkDestroyImage                DestroyImage               = nullptr;
        PFN_vkGetImageMemoryRequirements  GetImageMemReqs            = nullptr;
        PFN_vkAllocateMemory              AllocateMemory             = nullptr;
        PFN_vkFreeMemory                  FreeMemory                 = nullptr;
        PFN_vkBindImageMemory             BindImageMemory            = nullptr;
        PFN_vkDestroyCommandPool          DestroyCommandPool         = nullptr;
        PFN_vkQueueSubmit                 QueueSubmit                = nullptr;
        PFN_vkQueueWaitIdle               QueueWaitIdle              = nullptr;
        PFN_vkCreateSwapchainKHR          CreateSwapchain            = nullptr;
        PFN_vkDestroySwapchainKHR         DestroySwapchain           = nullptr;
        PFN_vkGetSwapchainImagesKHR       GetSwapchainImages         = nullptr;
        PFN_vkAcquireNextImageKHR         AcquireNextImage           = nullptr;
        PFN_vkQueuePresentKHR             QueuePresent               = nullptr;
        PFN_vkWaitForFences               WaitForFences              = nullptr;
        PFN_vkResetFences                 ResetFences                = nullptr;
    };

    uint32_t FindGraphicsQueueFamily(VkPhysicalDevice dev);
    bool     BuildDeviceTable(VkDevice dev);
    bool     EnsureLogicalDeviceUnlocked();
    bool     ChooseExtent(uint32_t& w, uint32_t& h) const;
    bool     CreateSwapchainLocked();
    void     DestroySwapchainLocked();
    bool     RunOffscreenProofLocked(std::string& err);
    bool     RecordAndSubmitFrame(uint32_t imageIndex,
                                  VkSemaphore acquireSem,
                                  VkSemaphore presentSem,
                                  VkClearColorValue color,
                                  VkCommandBuffer cb,
                                  VkFence inflight);

    GpuCapabilities m_caps;
    void* m_vulkanLib = nullptr;
    void* m_instance  = nullptr;
    void* m_physDev   = nullptr;

    // Rendering state (guarded by m_gpuMutex for structural changes)
    VkDevice     m_device        = VK_NULL_HANDLE;
    uint32_t     m_queueFamily   = 0xFFFFFFFFu;
    VkQueue      m_queue         = VK_NULL_HANDLE;
    DeviceTable  m_tbl{};
    ANativeWindow* m_window      = nullptr;
    VkSurfaceKHR m_surface       = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain   = VK_NULL_HANDLE;
    VkFormat     m_swapFormat    = VK_FORMAT_UNDEFINED;
    uint32_t     m_swapWidth     = 0;
    uint32_t     m_swapHeight    = 0;

    std::thread       m_renderThread;
    std::atomic<bool> m_rendering{false};

    mutable std::mutex m_gpuMutex;
    RenderStats        m_stats;
};

} // namespace PX5

#endif // PX5_VULKAN_DEVICE_H
