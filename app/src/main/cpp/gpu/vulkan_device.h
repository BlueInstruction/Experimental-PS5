#ifndef PX5_VULKAN_DEVICE_H
#define PX5_VULKAN_DEVICE_H

#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_core.h>

struct ANativeWindow;

namespace PX5::Gpu {
struct GuestShaderReflection;
struct GraphicsPipelineDesc;
struct MergedGraphicsReflection;
}

namespace PX5 {

/**
 * Real Vulkan enumeration results (no fake constants, all dlopen'd).
 */
struct GpuCapabilities {
    bool        initialized      = false;   ///< Whether Vulkan enumeration succeeded
    std::string apiVersionStr;              ///< Vulkan API version (e.g., "1.3.256")
    std::string deviceName;                 ///< Physical device name (e.g., "Adreno (TM) 750")
    std::string driverVersionStr;           ///< Driver version string
    uint32_t    deviceId          = 0;      ///< Vulkan device ID
    uint32_t    vendorId          = 0;      ///< Vulkan vendor ID
    uint32_t    physicalDevices   = 0;      ///< Number of physical devices enumerated
    std::string driverIdentity;             ///< VkPhysicalDeviceDriverProperties binding (driverID/driverName/Info) — the proof the enumerated device came from the loaded driver slot
    std::string lastError;                  ///< Non-empty => failure detail
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

/**
 * Vulkan GPU device wrapper with honest enumeration and rendering evidence.
 */
class VulkanGpuDevice {
public:
    /**
     * Returns the singleton VulkanGpuDevice instance.
     * @return Reference to singleton
     */
    static VulkanGpuDevice& GetInstance();

    /**
     * Initializes Vulkan: loader -> instance -> physical devices (Layer 1).
     * @return true if initialization succeeded, false otherwise
     */
    bool Initialize();

    /**
     * Shuts down Vulkan, releasing instance and loader.
     */
    void Shutdown();

    /**
     * Returns Vulkan capabilities discovered during initialization.
     * @return Reference to GpuCapabilities structure
     */
    const GpuCapabilities& GetCapabilities() const { return m_caps; }

    /**
     * Returns human-readable summary of GPU capabilities.
     * @return Formatted summary string
     */
    std::string GetSummaryString() const;

    /**
     * Ensures logical device and queue are created (idempotent, Layer 2).
     * @return true if device is ready, false otherwise
     */
    bool EnsureLogicalDevice();

    /**
     * Runs headless offscreen proof: alloc image + clear submit + fence wait.
     * In-process only; never call from fork-isolated child.
     * @param detailOut Output parameter receiving proof detail
     * @return true if proof passed, false otherwise
     */
    bool RunOffscreenClearProof(std::string& detailOut);

    /**
     * Runs fork-safe GPU proof with completely local Vulkan stack.
     * Builds fresh instance/device/queue, submits clear, destroys everything.
     * Safe in fork-isolated child (no inherited driver state touched).
     * @param detailOut Output parameter receiving proof detail
     * @return true if proof passed, false otherwise
     */
    bool RunSelfContainedProof(std::string& detailOut);

    /**
     * M7 gate proof (docs/milestones.md): the readback chain with expected
     * pixels. Self-contained and fork-safe by the same construction as
     * RunSelfContainedProof (fresh instance -> device -> own drm context),
     * extended with the full chain: IR Clear op (explicitly-labelled
     * synthetic GpuOpList) -> PlanVulkanCommands (gpu/vulkan_backend.cpp,
     * the host-locked planner) -> image clear -> submit -> fence ->
     * vkCmdCopyImageToBuffer into a host-visible buffer -> CPU readback ->
     * exact-pixel verification + SHA-256. A created device is NOT this
     * proof's evidence; the matched pixel count is.
     * @param detailOut Output parameter receiving proof detail
     * @return true if every readback byte matched the planned clear, false otherwise
     */
    bool RunM7ClearReadbackProof(std::string& detailOut);

    /**
     * M8 T7 device half — the compute dispatch proof. Self-contained and
     * fork-safe by the same construction as RunSelfContainedProof (fresh
     * instance -> device with bufferDeviceAddress -> own queue), extended
     * with the full recompiled-shader chain: the dispatch proof contract's
     * RDNA programs -> GuestShaderRecompiler (RDNA -> SPIR-V) -> shader
     * module + pipeline -> BDA buffers filled with the contract's input/
     * output bytes -> push-constant user data (PS5 descriptor roots +
     * selector) -> dispatch -> fence -> CPU readback -> VerifyOutput.
     * All six contract cases must match byte-exact; the first differing
     * thread is named in the detail line. A created pipeline is NOT the
     * evidence — the matched threads are.
     * @param detailOut Output parameter receiving proof detail
     * @return true if every contract case matched, false otherwise
     */
    bool RunM8ComputeReadbackProof(std::string& detailOut);

    // ---- M8 T5: guest shader objects (real Vulkan materialization) ------
    // Creates (or serves from the bounded device cache) a VkShaderModule
    // from recompiled SPIR-V words. Validation happens BEFORE the driver
    // sees anything (magic/version/count); every failure is named in
    // `err` and counted. Words come from GuestShaderRecompiler — this TU
    // never recompiles anything.
    bool CreateShaderModuleFromWords(const uint32_t* words, size_t wordCount,
                                     VkShaderModule* outModule,
                                     std::string& err);

    // Builds the descriptor-set layouts + pipeline layout the reflection
    // names, then the compute pipeline around the module. Layout objects
    // are owned here and destroyed with the pipeline. `vulkanStageFlags`
    // is the VkShaderStageFlagBits value the bindings and push-constant
    // range are declared for (compute = 0x20 on the current path).
    bool CreateComputePipelineFromReflection(
        const std::vector<uint32_t>& spirvWords,
        const PX5::Gpu::GuestShaderReflection& reflection,
        uint32_t vulkanStageFlags, VkPipeline* outPipeline,
        std::string& err);

    // ---- M8 T6: guest graphics pipelines (real Vulkan materialization) --
    // Validates the desc again at the boundary, merges the vertex+fragment
    // reflections into one binding table, then builds set layouts + pipeline
    // layout + render pass (GENERAL-layout LOAD/STORE attachments) + the
    // VkGraphicsPipeline (dynamic viewport+scissor, per-target blend array,
    // 1 sample, two stages, one push-constant block). Layouts and the render
    // pass are owned here and destroyed with the pipeline. The desc and
    // merge failures name their rule before the driver sees anything.
    bool CreateGraphicsPipelineFromReflection(
        const std::vector<uint32_t>& vertexSpirvWords,
        const PX5::Gpu::GuestShaderReflection& vertexReflection,
        const std::vector<uint32_t>& fragmentSpirvWords,
        const PX5::Gpu::GuestShaderReflection& fragmentReflection,
        const PX5::Gpu::GraphicsPipelineDesc& desc,
        VkPipeline* outPipeline,
        std::string& err);

    // Honest object accounting for the diagnostics panel.
    std::string GetShaderObjectStatsString() const;

    /**
     * Attaches ANativeWindow for on-screen rendering (SurfaceView lifecycle).
     * @param window Native window from Android
     * @return true if attachment succeeded, false otherwise
     */
    bool AttachWindowSurface(ANativeWindow* window);

    /**
     * Detaches the current window surface.
     */
    void DetachWindowSurface();

    /**
     * Starts the dedicated render loop thread.
     * @return true if render loop started, false otherwise
     */
    bool StartRenderLoop();

    /**
     * Stops the render loop thread.
     */
    void StopRenderLoop();

    /**
     * Stops render loop for GPU proof (external-synchronization contract).
     * @return true if loop was actually running, false otherwise
     */
    bool StopRenderLoopForProbe();

    /**
     * Resumes render loop after GPU proof (symmetric with StopRenderLoopForProbe).
     */
    void ResumeRenderLoopAfterProbe();

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
        // M8 T5 — guest shader objects. All core 1.0 entry points.
        PFN_vkCreateShaderModule          CreateShaderModule         = nullptr;
        PFN_vkDestroyShaderModule         DestroyShaderModule        = nullptr;
        PFN_vkCreateDescriptorSetLayout   CreateDescriptorSetLayout  = nullptr;
        PFN_vkDestroyDescriptorSetLayout  DestroyDescriptorSetLayout = nullptr;
        PFN_vkCreatePipelineLayout        CreatePipelineLayout       = nullptr;
        PFN_vkDestroyPipelineLayout       DestroyPipelineLayout      = nullptr;
        PFN_vkCreateComputePipelines      CreateComputePipelines     = nullptr;
        PFN_vkDestroyPipeline             DestroyPipeline            = nullptr;
        // M8 T6 — guest graphics pipelines. Core 1.0, hard-required like the
        // compute set: a driver that cannot host recompiled graphics shaders
        // is caught HERE, not at first draw.
        PFN_vkCreateRenderPass            CreateRenderPass           = nullptr;
        PFN_vkDestroyRenderPass           DestroyRenderPass          = nullptr;
        PFN_vkCreateGraphicsPipelines     CreateGraphicsPipelines    = nullptr;
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

    // ---- M8 T5 guest shader object state (guarded by m_gpuMutex) --------
    // Bounded drop-all caches (same policy as the recompiler service):
    // key = FNV-1a over the exact inputs. Modules: words. Pipelines:
    // words + bindings + stage flags. Owned layouts travel beside their
    // pipeline key so DestroyShaderObjects can free everything exactly.
    static constexpr size_t kMaxShaderModules = 128;
    static constexpr size_t kMaxComputePipelines = 64;
    static constexpr size_t kMaxGraphicsPipelines = 64;
    std::unordered_map<uint64_t, VkShaderModule> m_moduleCache;
    struct PipelineObjects {
        VkPipeline               pipeline = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> setLayouts;
        VkPipelineLayout         layout = VK_NULL_HANDLE;
        // The VkShaderModule is NOT tracked here: the module cache owns all
        // modules (sharing across pipelines), this entry owns the rest.
    };
    std::unordered_map<uint64_t, PipelineObjects> m_pipelineCache;
    // M8 T6 — graphics pipelines own their render pass too (it is built per
    // pipeline from the desc's attachment formats).
    struct GraphicsPipelineObjects {
        VkPipeline               pipeline = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> setLayouts;
        VkPipelineLayout         layout = VK_NULL_HANDLE;
        VkRenderPass             renderPass = VK_NULL_HANDLE;
    };
    std::unordered_map<uint64_t, GraphicsPipelineObjects> m_gfxPipelineCache;
    struct ShaderObjectStats {
        std::atomic<uint64_t> modulesCreated{0};
        std::atomic<uint64_t> moduleHits{0};
        std::atomic<uint64_t> moduleEvictions{0};
        std::atomic<uint64_t> pipelinesCreated{0};
        std::atomic<uint64_t> pipelineHits{0};
        std::atomic<uint64_t> pipelineEvictions{0};
        std::atomic<uint64_t> gfxCreated{0};
        std::atomic<uint64_t> gfxHits{0};
        std::atomic<uint64_t> gfxEvictions{0};
        std::atomic<uint64_t> failures{0};
    };
    ShaderObjectStats m_shaderObjStats;
    void DestroyShaderObjectsLocked();
    // Caller must hold m_gpuMutex (public wrapper CreateShaderModuleFromWords
    // locks; the pipeline path calls this directly — std::mutex is not
    // recursive).
    bool CreateShaderModuleLocked(const uint32_t* words, size_t wordCount,
                                  VkShaderModule* outModule, std::string& err);
    uint64_t HashWords(const uint32_t* words, size_t wordCount) const;
    uint64_t HashReflection(const std::vector<uint32_t>& spirvWords,
                            const PX5::Gpu::GuestShaderReflection& reflection,
                            uint32_t vulkanStageFlags) const;
    // M8 T6 — 'gpx1'-domain key (no collision with the compute reflection
    // keys over identical inputs). Mixes both stages' words, the merged
    // binding table, the push window, and every desc field the object
    // depends on.
    uint64_t HashGraphicsPipeline(
        const std::vector<uint32_t>& vertexSpirvWords,
        const std::vector<uint32_t>& fragmentSpirvWords,
        const PX5::Gpu::MergedGraphicsReflection& merged,
        const PX5::Gpu::GraphicsPipelineDesc& desc) const;

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
    // v1.18 — true only when VK_KHR_swapchain was enabled at device
    // creation (BuildDeviceTable resolved all four swapchain entry
    // points). The on-screen path refuses to run without it, honestly.
    bool         m_swapchainExtAvailable = false;

    std::thread       m_renderThread;
    std::atomic<bool> m_rendering{false};

    mutable std::mutex m_gpuMutex;
    // Vulkan requires EXTERNAL synchronization of each queue. The render
    // thread AND the offscreen proof both submit to m_queue; without this
    // lock they race driver-internal queue state (2026-08-30 device SIGSEGV
    // si_addr=0x0 at EmuScreen entry — three identical deaths).
    std::mutex       m_queueMutex;
    RenderStats        m_stats;
};

} // namespace PX5

#endif // PX5_VULKAN_DEVICE_H
