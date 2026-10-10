// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T6 — graphics pipeline desc: validation, vertex-format table, merge.
// Pure data + pure functions; no Vulkan calls, no driver contact. Every
// failure names the rule it rejected so a device log pinpoints the desc field
// the guest state filled wrong.
#include "graphics_pipeline_desc.h"

#include <algorithm>
#include <map>

namespace PX5::Gpu {
namespace {

// The allowlist: exactly 35 vertex-capable VkFormat numerics with byte sizes.
// Locked as a double-entry ledger by the host gate (tools/hosttests/
// graphics_pipeline_desc_test.cpp) — the test hardcodes the same numbers.
struct VertexFormatEntry {
    uint32_t format;
    uint32_t sizeBytes;
};

constexpr VertexFormatEntry kVertexFormats[35] = {
    {VkNum::kFormatR8Unorm,             1},
    {VkNum::kFormatR8Snorm,             1},
    {VkNum::kFormatR8Uint,              1},
    {VkNum::kFormatR8Sint,              1},
    {VkNum::kFormatR8G8Unorm,           2},
    {VkNum::kFormatR8G8Snorm,           2},
    {VkNum::kFormatR8G8Uint,            2},
    {VkNum::kFormatR8G8Sint,            2},
    {VkNum::kFormatR8G8B8Unorm,         3},
    {VkNum::kFormatR8G8B8Snorm,         3},
    {VkNum::kFormatR8G8B8A8Unorm,       4},
    {VkNum::kFormatR8G8B8A8Snorm,       4},
    {VkNum::kFormatR8G8B8A8Uint,        4},
    {VkNum::kFormatR8G8B8A8Sint,        4},
    {VkNum::kFormatR8G8B8A8Srgb,        4},
    {VkNum::kFormatB8G8R8A8Unorm,       4},
    {VkNum::kFormatA8B8G8R8UnormPack32, 4},
    {VkNum::kFormatA8B8G8R8UintPack32,  4},
    {VkNum::kFormatA2B10G10R10UnormPack32, 4},
    {VkNum::kFormatR16Unorm,            2},
    {VkNum::kFormatR16Snorm,            2},
    {VkNum::kFormatR16Uint,             2},
    {VkNum::kFormatR16Sfloat,           2},
    {VkNum::kFormatR16G16Unorm,         4},
    {VkNum::kFormatR16G16Snorm,         4},
    {VkNum::kFormatR16G16Sfloat,        4},
    {VkNum::kFormatR16G16B16A16Sfloat,  8},
    {VkNum::kFormatR32Uint,             4},
    {VkNum::kFormatR32Sint,             4},
    {VkNum::kFormatR32Sfloat,           4},
    {VkNum::kFormatR32G32Uint,          8},
    {VkNum::kFormatR32G32Sint,          8},
    {VkNum::kFormatR32G32Sfloat,        8},
    {VkNum::kFormatR32G32B32Sfloat,     12},
    {VkNum::kFormatR32G32B32A32Sfloat,  16},
};

bool BlendFactorInRange(uint32_t f) { return f <= VkNum::kBlendFactorMax; }
bool BlendOpInRange(uint32_t op) { return op <= VkNum::kBlendOpMaxValue; }
bool CompareOpInRange(uint32_t op) { return op <= VkNum::kCompareMax; }
bool StencilOpInRange(uint32_t op) { return op <= VkNum::kStencilOpMax; }

// One stencil face's ops + compare op are range-checked in every state.
bool ValidateStencilFace(const GraphicsStencilFace& face, const char* which,
                         std::string& err) {
    if (!StencilOpInRange(face.failOp) || !StencilOpInRange(face.passOp) ||
        !StencilOpInRange(face.depthFailOp)) {
        err = std::string("stencil ") + which +
              " face op outside the Vulkan 0..7 range";
        return false;
    }
    if (!CompareOpInRange(face.compareOp)) {
        err = std::string("stencil ") + which +
              " face compareOp outside the Vulkan 0..7 range";
        return false;
    }
    return true;
}

}  // namespace

bool VertexFormatSize(uint32_t format, uint32_t& sizeOut) {
    for (const auto& e : kVertexFormats) {
        if (e.format == format) {
            sizeOut = e.sizeBytes;
            return true;
        }
    }
    return false;
}

bool IsDepthFormat(uint32_t format) {
    return format == VkNum::kFormatD16Unorm ||
           format == VkNum::kFormatD32Sfloat ||
           format == VkNum::kFormatD24UnormS8Uint ||
           format == VkNum::kFormatD32SfloatS8Uint;
}

bool ValidateGraphicsPipelineDesc(const GraphicsPipelineDesc& desc,
                                  std::string& err) {
    err.clear();

    // --- topology ---------------------------------------------------------
    if (desc.topology > VkNum::kTopologyMax) {
        err = "topology " + std::to_string(desc.topology) +
              " outside the Vulkan 0..5 range";
        return false;
    }

    // --- color targets ------------------------------------------------------
    if (desc.colorTargetCount == 0u) {
        err = "color target count is zero";
        return false;
    }
    if (desc.colorTargetCount > kMaxColorTargets) {
        err = "color target count " + std::to_string(desc.colorTargetCount) +
              " exceeds the " + std::to_string(kMaxColorTargets) +
              " attachment limit";
        return false;
    }
    if (desc.blends.size() != desc.colorTargetCount) {
        err = "blend state count " + std::to_string(desc.blends.size()) +
              " does not match the " +
              std::to_string(desc.colorTargetCount) + " color targets";
        return false;
    }

    // --- blend array (range rules cover all targets) ------------------------
    for (uint32_t i = 0; i < desc.blends.size(); ++i) {
        const GraphicsTargetBlend& b = desc.blends[i];
        if (!BlendFactorInRange(b.srcColorFactor) ||
            !BlendFactorInRange(b.dstColorFactor) ||
            !BlendFactorInRange(b.srcAlphaFactor) ||
            !BlendFactorInRange(b.dstAlphaFactor)) {
            err = "blend target " + std::to_string(i) +
                  " factor outside the Vulkan 0..18 range";
            return false;
        }
        if (!BlendOpInRange(b.colorBlendOp) || !BlendOpInRange(b.alphaBlendOp)) {
            err = "blend target " + std::to_string(i) +
                  " blendOp outside the Vulkan 0..4 range";
            return false;
        }
    }

    // --- color formats ------------------------------------------------------
    for (uint32_t i = 0; i < desc.colorTargetCount; ++i) {
        const uint32_t f = desc.colorFormats[i];
        if (f == VkNum::kFormatUndefined) {
            err = "color target " + std::to_string(i) +
                  " format is UNDEFINED (zero)";
            return false;
        }
        if (IsDepthFormat(f)) {
            err = "depth format " + std::to_string(f) +
                  " used as color target " + std::to_string(i);
            return false;
        }
        for (uint32_t j = 0; j < i; ++j) {
            if (desc.colorFormats[j] == f) {
                err = "color formats " + std::to_string(j) + " and " +
                      std::to_string(i) + " are the same format " +
                      std::to_string(f);
                return false;
            }
        }
    }

    // --- depth format -------------------------------------------------------
    if (desc.depthFormat != 0u && !IsDepthFormat(desc.depthFormat)) {
        err = "depth format " + std::to_string(desc.depthFormat) +
              " is not one of D16/D32/D24S8/D32S8";
        return false;
    }

    // --- depth/stencil rule set ----------------------------------------------
    if (!CompareOpInRange(desc.depthStencil.depthCompareOp)) {
        err = "depthCompareOp " +
              std::to_string(desc.depthStencil.depthCompareOp) +
              " outside the Vulkan 0..7 range";
        return false;
    }
    if (desc.depthStencil.depthWriteEnable &&
        !desc.depthStencil.depthTestEnable) {
        err = "depthWriteEnable requires depthTestEnable";
        return false;
    }
    if (desc.depthStencil.depthBoundsTestEnable) {
        err = "depth bounds test requires a device feature this layer "
              "does not enable";
        return false;
    }
    if (desc.depthStencil.stencilTestEnable) {
        if (!ValidateStencilFace(desc.depthStencil.front, "front", err) ||
            !ValidateStencilFace(desc.depthStencil.back, "back", err)) {
            return false;
        }
    }

    // --- dynamic viewport+scissor contract ----------------------------------
    if (!desc.dynamicViewportScissor) {
        err = "static viewport/scissor is not supported by this layer "
              "(dynamic only)";
        return false;
    }

    // --- vertex input --------------------------------------------------------
    for (uint32_t i = 0; i < desc.vertexBindings.size(); ++i) {
        for (uint32_t j = 0; j < i; ++j) {
            if (desc.vertexBindings[j].binding ==
                desc.vertexBindings[i].binding) {
                err = "vertex binding " +
                      std::to_string(desc.vertexBindings[i].binding) +
                      " declared twice";
                return false;
            }
        }
    }
    for (const GraphicsVertexAttribute& a : desc.vertexAttributes) {
        const GraphicsVertexBinding* bound = nullptr;
        for (const GraphicsVertexBinding& b : desc.vertexBindings) {
            if (b.binding == a.binding) {
                bound = &b;
                break;
            }
        }
        if (bound == nullptr) {
            err = "attribute location " + std::to_string(a.location) +
                  " references undeclared binding " +
                  std::to_string(a.binding);
            return false;
        }
        uint32_t size = 0;
        if (!VertexFormatSize(a.format, size)) {
            err = "attribute location " + std::to_string(a.location) +
                  " format " + std::to_string(a.format) +
                  " is not in the vertex format allowlist";
            return false;
        }
        if (bound->stride == 0u) {
            err = "vertex binding " + std::to_string(bound->binding) +
                  " has stride 0 but attributes reference it";
            return false;
        }
        const uint64_t end =
            static_cast<uint64_t>(a.offset) + static_cast<uint64_t>(size);
        if (end > static_cast<uint64_t>(bound->stride)) {
            err = "attribute location " + std::to_string(a.location) +
                  " (format " + std::to_string(a.format) + ", size " +
                  std::to_string(size) + ", offset " +
                  std::to_string(a.offset) + ") overruns binding " +
                  std::to_string(bound->binding) + " stride " +
                  std::to_string(bound->stride);
            return false;
        }
    }
    for (uint32_t i = 0; i < desc.vertexAttributes.size(); ++i) {
        for (uint32_t j = 0; j < i; ++j) {
            if (desc.vertexAttributes[j].location ==
                desc.vertexAttributes[i].location) {
                err = "attribute location " +
                      std::to_string(desc.vertexAttributes[i].location) +
                      " declared twice";
                return false;
            }
        }
    }

    return true;
}

bool MergeGraphicsReflections(const GuestShaderReflection& vertex,
                              const GuestShaderReflection& fragment,
                              MergedGraphicsReflection& out,
                              std::string& err) {
    err.clear();
    out = MergedGraphicsReflection{};

    // (set, binding) -> merged entry; insertion order is irrelevant because
    // the output is sorted before returning.
    std::map<std::pair<uint32_t, uint32_t>, MergedGraphicsReflection::Binding>
        merged;

    const auto absorb = [&merged, &err, &out](
                            const GuestShaderReflection& r, uint32_t stageBit,
                            const char* stageName) -> bool {
        for (const GuestShaderBinding& b : r.bindings) {
            const auto key = std::make_pair(b.set, b.binding);
            auto it = merged.find(key);
            if (it == merged.end()) {
                MergedGraphicsReflection::Binding m;
                m.type = b.type;
                m.set = b.set;
                m.binding = b.binding;
                m.count = b.count;
                m.readOnly = b.readOnly;
                m.stageFlags = stageBit;
                merged.emplace(key, m);
                continue;
            }
            MergedGraphicsReflection::Binding& m = it->second;
            if (m.type != b.type) {
                err = std::string("binding set ") + std::to_string(b.set) +
                      " binding " + std::to_string(b.binding) +
                      " declared as type " + std::to_string(m.type) +
                      " and type " + std::to_string(b.type) + " (" +
                      stageName + ")";
                return false;
            }
            if (m.count != b.count) {
                err = std::string("binding set ") + std::to_string(b.set) +
                      " binding " + std::to_string(b.binding) +
                      " declared with count " + std::to_string(m.count) +
                      " and count " + std::to_string(b.count) + " (" +
                      stageName + ")";
                return false;
            }
            m.readOnly = m.readOnly && b.readOnly;
            m.stageFlags |= stageBit;
        }
        if (r.pushConstantSizeBytes > 0u) {
            out.pushConstantSizeBytes = std::max(out.pushConstantSizeBytes,
                                                 r.pushConstantSizeBytes);
            out.pushConstantStageFlags |= stageBit;
        }
        return true;
    };

    if (!absorb(vertex, VkNum::kStageVertexBit, "vertex")) return false;
    if (!absorb(fragment, VkNum::kStageFragmentBit, "fragment")) return false;

    out.bindings.reserve(merged.size());
    for (auto& kv : merged) out.bindings.push_back(kv.second);
    // std::map iterates in (set, binding) order already; sort anyway so the
    // guarantee survives a container swap.
    std::sort(out.bindings.begin(), out.bindings.end(),
              [](const MergedGraphicsReflection::Binding& a,
                 const MergedGraphicsReflection::Binding& b2) {
                  return std::make_pair(a.set, a.binding) <
                         std::make_pair(b2.set, b2.binding);
              });
    return true;
}

}  // namespace PX5::Gpu
