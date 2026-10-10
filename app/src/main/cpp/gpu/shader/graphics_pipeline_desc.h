// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T6 — Vulkan-free graphics pipeline description, validation, merge.
//
// WHAT THIS IS
//   The app-owned vocabulary for guest graphics pipelines: a desc struct the
//   GPU command layer can fill from decoded GnmState state, a validator in
//   which EVERY rejected rule names itself, and a reflection merge that
//   combines the recompiled vertex + fragment outputs into the one binding
//   table a VkPipelineLayout is built from.
//
//   Vulkan types never appear here. Every numeric value is the STABLE PUBLIC
//   Vulkan enumeration value pinned from vulkan_core.h (locked by the host
//   gate as a double-entry ledger), so the host gate can lock behaviour
//   without a Vulkan SDK and the device TU can cast straight to the enum.
//
//   Assembly shape follows the AnyPS5 libSceAgcDriver Graphics pipeline
//   contract (GPL-2.0-only): dynamic viewport+scissor, per-target blend
//   array, GENERAL-layout LOAD/STORE attachments, 1 sample, two stages, one
//   push-constant block.
//
// WHAT THIS IS NOT
//   No Vulkan calls, no driver contact, no cache — materialization is
//   gpu/vulkan_device.cpp's job; this file is pure data + pure functions.
#ifndef PX5_GPU_SHADER_GRAPHICS_PIPELINE_DESC_H
#define PX5_GPU_SHADER_GRAPHICS_PIPELINE_DESC_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "shader_recompiler_service.h"  // GuestShaderReflection (header only)

namespace PX5::Gpu {

// ---------------------------------------------------------------------------
// Stable public Vulkan numeric values (pinned from vulkan_core.h; the host
// gate locks these numbers so a silent header drift is caught, not guessed).
// ---------------------------------------------------------------------------
namespace VkNum {

// VkPrimitiveTopology
constexpr uint32_t kTopologyPointList    = 0;
constexpr uint32_t kTopologyLineList     = 1;
constexpr uint32_t kTopologyLineStrip    = 2;
constexpr uint32_t kTopologyTriangleList = 3;
constexpr uint32_t kTopologyTriangleStrip = 4;
constexpr uint32_t kTopologyTriangleFan  = 5;
constexpr uint32_t kTopologyMax          = 5;

// VkCompareOp
constexpr uint32_t kCompareNever         = 0;
constexpr uint32_t kCompareLess          = 1;
constexpr uint32_t kCompareEqual         = 2;
constexpr uint32_t kCompareLessEqual     = 3;
constexpr uint32_t kCompareGreater       = 4;
constexpr uint32_t kCompareNotEqual      = 5;
constexpr uint32_t kCompareGreaterEqual  = 6;
constexpr uint32_t kCompareAlways        = 7;
constexpr uint32_t kCompareMax           = 7;

// VkStencilOp
constexpr uint32_t kStencilKeep              = 0;
constexpr uint32_t kStencilZero              = 1;
constexpr uint32_t kStencilReplace           = 2;
constexpr uint32_t kStencilIncrementClamp    = 3;
constexpr uint32_t kStencilDecrementClamp    = 4;
constexpr uint32_t kStencilInvert            = 5;
constexpr uint32_t kStencilIncrementWrap     = 6;
constexpr uint32_t kStencilDecrementWrap     = 7;
constexpr uint32_t kStencilOpMax             = 7;

// VkBlendFactor
constexpr uint32_t kBlendZero                    = 0;
constexpr uint32_t kBlendOne                     = 1;
constexpr uint32_t kBlendSrcColor                = 2;
constexpr uint32_t kBlendOneMinusSrcColor        = 3;
constexpr uint32_t kBlendDstColor                = 4;
constexpr uint32_t kBlendOneMinusDstColor        = 5;
constexpr uint32_t kBlendSrcAlpha                = 6;
constexpr uint32_t kBlendOneMinusSrcAlpha        = 7;
constexpr uint32_t kBlendDstAlpha                = 8;
constexpr uint32_t kBlendOneMinusDstAlpha        = 9;
constexpr uint32_t kBlendConstantColor           = 10;
constexpr uint32_t kBlendOneMinusConstantColor   = 11;
constexpr uint32_t kBlendConstantAlpha           = 12;
constexpr uint32_t kBlendOneMinusConstantAlpha   = 13;
constexpr uint32_t kBlendSrcAlphaSaturate        = 14;
constexpr uint32_t kBlendSrc1Color               = 15;
constexpr uint32_t kBlendOneMinusSrc1Color       = 16;
constexpr uint32_t kBlendSrc1Alpha               = 17;
constexpr uint32_t kBlendOneMinusSrc1Alpha       = 18;
constexpr uint32_t kBlendFactorMax               = 18;

// VkBlendOp
constexpr uint32_t kBlendOpAdd              = 0;
constexpr uint32_t kBlendOpSubtract         = 1;
constexpr uint32_t kBlendOpReverseSubtract  = 2;
constexpr uint32_t kBlendOpMin              = 3;
constexpr uint32_t kBlendOpMax              = 4;
constexpr uint32_t kBlendOpMaxValue         = 4;

// VkColorComponentFlags
constexpr uint32_t kColorComponentR = 1u << 0;
constexpr uint32_t kColorComponentG = 1u << 1;
constexpr uint32_t kColorComponentB = 1u << 2;
constexpr uint32_t kColorComponentA = 1u << 3;
constexpr uint32_t kColorComponentAll = 0xFu;

// VkShaderStageFlagBits (used by the merge's stage masks)
constexpr uint32_t kStageVertexBit   = 1u << 0;  // 0x1
constexpr uint32_t kStageFragmentBit = 1u << 4;  // 0x10
constexpr uint32_t kStageComputeBit  = 1u << 5;  // 0x20

// VkFormat — the subset this layer speaks (see VertexFormatSize below).
constexpr uint32_t kFormatUndefined            = 0;
constexpr uint32_t kFormatR8Unorm              = 9;
constexpr uint32_t kFormatR8Snorm              = 10;
constexpr uint32_t kFormatR8Uint               = 13;
constexpr uint32_t kFormatR8Sint               = 14;
constexpr uint32_t kFormatR8G8Unorm            = 16;
constexpr uint32_t kFormatR8G8Snorm            = 17;
constexpr uint32_t kFormatR8G8Uint             = 20;
constexpr uint32_t kFormatR8G8Sint             = 21;
constexpr uint32_t kFormatR8G8B8Unorm          = 23;
constexpr uint32_t kFormatR8G8B8Snorm          = 24;
constexpr uint32_t kFormatR8G8B8A8Unorm        = 37;
constexpr uint32_t kFormatR8G8B8A8Snorm        = 38;
constexpr uint32_t kFormatR8G8B8A8Uint         = 41;
constexpr uint32_t kFormatR8G8B8A8Sint         = 42;
constexpr uint32_t kFormatR8G8B8A8Srgb         = 43;
constexpr uint32_t kFormatB8G8R8A8Unorm        = 44;
constexpr uint32_t kFormatA8B8G8R8UnormPack32  = 51;
constexpr uint32_t kFormatA8B8G8R8UintPack32   = 55;
constexpr uint32_t kFormatA2B10G10R10UnormPack32 = 64;
constexpr uint32_t kFormatR16Unorm             = 70;
constexpr uint32_t kFormatR16Snorm             = 71;
constexpr uint32_t kFormatR16Uint              = 74;
constexpr uint32_t kFormatR16Sfloat            = 76;
constexpr uint32_t kFormatR16G16Unorm          = 77;
constexpr uint32_t kFormatR16G16Snorm          = 78;
constexpr uint32_t kFormatR16G16Sfloat         = 83;
constexpr uint32_t kFormatR16G16B16A16Sfloat   = 97;
constexpr uint32_t kFormatR32Uint              = 98;
constexpr uint32_t kFormatR32Sint              = 99;
constexpr uint32_t kFormatR32Sfloat            = 100;
constexpr uint32_t kFormatR32G32Uint           = 101;
constexpr uint32_t kFormatR32G32Sint           = 102;
constexpr uint32_t kFormatR32G32Sfloat         = 103;
constexpr uint32_t kFormatR32G32B32Sfloat      = 106;
constexpr uint32_t kFormatR32G32B32A32Sfloat   = 109;
// Depth-capable formats (attachment only — never a vertex or color format).
constexpr uint32_t kFormatD16Unorm             = 124;
constexpr uint32_t kFormatD32Sfloat            = 126;
constexpr uint32_t kFormatD24UnormS8Uint       = 129;
constexpr uint32_t kFormatD32SfloatS8Uint      = 130;

// VkDynamicState
constexpr uint32_t kDynamicStateViewport = 1;
constexpr uint32_t kDynamicStateScissor  = 2;

}  // namespace VkNum

/// Largest color attachment count this layer accepts (Vulkan core minimum
/// guarantee is 4; 8 is the common desktop/mobile hardware count).
constexpr uint32_t kMaxColorTargets = 8;

// ---------------------------------------------------------------------------
// The description. Defaults describe the honest minimal pipeline: TriangleList,
// opaque blend (no blending), depth/stencil off, dynamic viewport+scissor.
// ---------------------------------------------------------------------------
struct GraphicsVertexBinding {
    uint32_t binding = 0;
    uint32_t stride = 0;          // bytes per vertex; 0 rejected when used
    bool perInstance = false;     // VkVertexInputRate: 0=Vertex, 1=Instance
};

struct GraphicsVertexAttribute {
    uint32_t location = 0;        // shader input location
    uint32_t binding = 0;         // GraphicsVertexBinding::binding
    uint32_t format = 0;          // VkFormat numeric (allowlist below)
    uint32_t offset = 0;          // bytes into the binding's stride
};

struct GraphicsTargetBlend {
    bool blendEnable = false;
    uint32_t srcColorFactor = VkNum::kBlendOne;
    uint32_t dstColorFactor = VkNum::kBlendZero;
    uint32_t colorBlendOp   = VkNum::kBlendOpAdd;
    uint32_t srcAlphaFactor = VkNum::kBlendOne;
    uint32_t dstAlphaFactor = VkNum::kBlendZero;
    uint32_t alphaBlendOp   = VkNum::kBlendOpAdd;
    uint32_t colorWriteMask = VkNum::kColorComponentAll;
};

struct GraphicsStencilFace {
    uint32_t failOp      = VkNum::kStencilKeep;
    uint32_t passOp      = VkNum::kStencilKeep;
    uint32_t depthFailOp = VkNum::kStencilKeep;
    uint32_t compareOp   = VkNum::kCompareNever;
    uint32_t compareMask = 0xFFu;
    uint32_t writeMask   = 0xFFu;
    uint32_t reference   = 0;
};

struct GraphicsDepthStencilDesc {
    bool depthTestEnable = false;
    bool depthWriteEnable = false;
    uint32_t depthCompareOp = VkNum::kCompareAlways;
    bool depthBoundsTestEnable = false;  // rejected: needs a device feature
    bool stencilTestEnable = false;
    GraphicsStencilFace front;
    GraphicsStencilFace back;
};

struct GraphicsPipelineDesc {
    uint32_t topology = VkNum::kTopologyTriangleList;
    std::vector<GraphicsVertexBinding> vertexBindings;
    std::vector<GraphicsVertexAttribute> vertexAttributes;
    uint32_t colorTargetCount = 1;
    std::array<uint32_t, kMaxColorTargets> colorFormats{};
    // 0 = no depth attachment; otherwise D16/D32/D24S8/D32S8 (VkNum above).
    uint32_t depthFormat = 0;
    std::vector<GraphicsTargetBlend> blends;  // size() == colorTargetCount
    GraphicsDepthStencilDesc depthStencil;
    // This layer materializes the AnyPS5 assembly shape: dynamic
    // viewport+scissor only. false is a named rejection, not a guess.
    bool dynamicViewportScissor = true;
};

// ---------------------------------------------------------------------------
// Vertex format allowlist: exactly 35 formats, each with its byte size.
// Returns true and fills sizeOut for a format the layer accepts.
// ---------------------------------------------------------------------------
bool VertexFormatSize(uint32_t format, uint32_t& sizeOut);

/// True when the format is one of the four depth-capable attachment formats.
bool IsDepthFormat(uint32_t format);

// ---------------------------------------------------------------------------
// Validation. Every rejected rule NAMES ITSELF in `err` (the host gate locks
// each message's distinctive fragment). Returns true when `desc` is valid.
// ---------------------------------------------------------------------------
bool ValidateGraphicsPipelineDesc(const GraphicsPipelineDesc& desc,
                                  std::string& err);

// ---------------------------------------------------------------------------
// Reflection merge: one binding table out of the two recompiled stages.
//   - union of (set, binding); a key present in both stages gets the OR of
//     its stage masks (a VS+FS resource),
//   - same key with a different type or count is a NAMED refusal,
//   - readOnly ANDs (either stage writing makes the resource writable),
//   - the push window is the MAX size across stages and the OR of the
//     stages that declare one,
//   - output is sorted by (set, binding).
// ---------------------------------------------------------------------------
struct MergedGraphicsReflection {
    struct Binding {
        uint32_t type = 0;       // VkDescriptorType numeric (GuestShaderBinding)
        uint32_t set = 0;
        uint32_t binding = 0;
        uint32_t count = 0;
        bool readOnly = false;
        uint32_t stageFlags = 0; // VkNum::kStage*Bit OR
    };
    std::vector<Binding> bindings;        // sorted by (set, binding)
    uint32_t pushConstantSizeBytes = 0;   // max across stages
    uint32_t pushConstantStageFlags = 0;  // OR of stages declaring a window
};

bool MergeGraphicsReflections(const GuestShaderReflection& vertex,
                              const GuestShaderReflection& fragment,
                              MergedGraphicsReflection& out,
                              std::string& err);

}  // namespace PX5::Gpu

#endif  // PX5_GPU_SHADER_GRAPHICS_PIPELINE_DESC_H
