// SPDX-License-Identifier: GPL-2.0-only
// PX5 M8 T6 host gate — graphics_pipeline_desc locked behaviour.
//
// Four locks:
//   1. NUMERIC PINS — every VkNum constant against hardcoded literals (a
//      double-entry ledger: if a header or the table drifts, this fails).
//   2. MERGE SEMANTICS — union across stages, stage-mask OR, type/count
//      conflict refusals name the (set, binding), readOnly AND, push-window
//      max + stage OR, (set, binding)-sorted output.
//   3. HAPPY PATH — the desc a 2D quad draw would build validates.
//   4. NAMED REJECTIONS — fourteen invalid descs, each asserting the rule's
//      distinctive message fragment.
#include "gpu/shader/graphics_pipeline_desc.h"

#include <cstdint>
#include <cstdio>
#include <string>

using namespace PX5::Gpu;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (ok) {
        std::printf("  ok  %s\n", what);
        return;
    }
    ++g_failures;
    std::printf("FAIL  %s\n", what);
}

void CheckRejected(const GraphicsPipelineDesc& desc,
                   const std::string& fragment, const char* what) {
    std::string err;
    const bool valid = ValidateGraphicsPipelineDesc(desc, err);
    if (!valid && err.find(fragment) != std::string::npos) {
        std::printf("  ok  %s (\"%s\")\n", what, err.c_str());
        return;
    }
    ++g_failures;
    std::printf("FAIL  %s — valid=%d err=\"%s\" (wanted fragment \"%s\")\n",
                what, valid ? 1 : 0, err.c_str(), fragment.c_str());
}

GuestShaderReflection Reflect(std::vector<GuestShaderBinding> bindings,
                              uint32_t pushBytes) {
    GuestShaderReflection r;
    r.bindings = std::move(bindings);
    r.pushConstantSizeBytes = pushBytes;
    return r;
}

}  // namespace

int main() {
    // ---- 1. numeric pins ---------------------------------------------------
    Check(VkNum::kTopologyPointList == 0 && VkNum::kTopologyLineList == 1 &&
              VkNum::kTopologyLineStrip == 2 &&
              VkNum::kTopologyTriangleList == 3 &&
              VkNum::kTopologyTriangleStrip == 4 &&
              VkNum::kTopologyTriangleFan == 5,
          "topology pins 0..5");
    Check(VkNum::kCompareNever == 0 && VkNum::kCompareLess == 1 &&
              VkNum::kCompareEqual == 2 && VkNum::kCompareLessEqual == 3 &&
              VkNum::kCompareGreater == 4 && VkNum::kCompareNotEqual == 5 &&
              VkNum::kCompareGreaterEqual == 6 && VkNum::kCompareAlways == 7,
          "compare-op pins 0..7");
    Check(VkNum::kStencilKeep == 0 && VkNum::kStencilZero == 1 &&
              VkNum::kStencilReplace == 2 &&
              VkNum::kStencilIncrementClamp == 3 &&
              VkNum::kStencilDecrementClamp == 4 &&
              VkNum::kStencilInvert == 5 &&
              VkNum::kStencilIncrementWrap == 6 &&
              VkNum::kStencilDecrementWrap == 7,
          "stencil-op pins 0..7");
    Check(VkNum::kBlendZero == 0 && VkNum::kBlendOne == 1 &&
              VkNum::kBlendSrcColor == 2 &&
              VkNum::kBlendOneMinusSrcColor == 3 &&
              VkNum::kBlendDstColor == 4 &&
              VkNum::kBlendOneMinusDstColor == 5 &&
              VkNum::kBlendSrcAlpha == 6 &&
              VkNum::kBlendOneMinusSrcAlpha == 7 &&
              VkNum::kBlendDstAlpha == 8 &&
              VkNum::kBlendOneMinusDstAlpha == 9 &&
              VkNum::kBlendConstantColor == 10 &&
              VkNum::kBlendOneMinusConstantColor == 11 &&
              VkNum::kBlendConstantAlpha == 12 &&
              VkNum::kBlendOneMinusConstantAlpha == 13 &&
              VkNum::kBlendSrcAlphaSaturate == 14 &&
              VkNum::kBlendSrc1Color == 15 &&
              VkNum::kBlendOneMinusSrc1Color == 16 &&
              VkNum::kBlendSrc1Alpha == 17 &&
              VkNum::kBlendOneMinusSrc1Alpha == 18,
          "blend-factor pins 0..18");
    Check(VkNum::kBlendOpAdd == 0 && VkNum::kBlendOpSubtract == 1 &&
              VkNum::kBlendOpReverseSubtract == 2 &&
              VkNum::kBlendOpMin == 3 && VkNum::kBlendOpMax == 4,
          "blend-op pins 0..4");
    Check(VkNum::kStageVertexBit == 0x1 && VkNum::kStageFragmentBit == 0x10 &&
              VkNum::kStageComputeBit == 0x20,
          "shader-stage bit pins 0x1/0x10/0x20");
    Check(VkNum::kFormatR8G8B8A8Unorm == 37 &&
              VkNum::kFormatB8G8R8A8Unorm == 44 &&
              VkNum::kFormatR16G16Sfloat == 83 &&
              VkNum::kFormatR32Uint == 98 && VkNum::kFormatR32Sfloat == 100 &&
              VkNum::kFormatR32G32Sfloat == 103 &&
              VkNum::kFormatR32G32B32A32Sfloat == 109 &&
              VkNum::kFormatD16Unorm == 124 &&
              VkNum::kFormatD32Sfloat == 126 &&
              VkNum::kFormatD24UnormS8Uint == 129 &&
              VkNum::kFormatD32SfloatS8Uint == 130,
          "format pins (color, vertex, depth)");
    Check(VkNum::kDynamicStateViewport == 1 &&
              VkNum::kDynamicStateScissor == 2,
          "dynamic-state pins");

    // Vertex-format table: size correctness on representative entries plus
    // the exact allowlist population (35 entries).
    uint32_t size = 0;
    Check(VertexFormatSize(VkNum::kFormatR8Unorm, size) && size == 1,
          "R8_UNORM is 1 byte");
    Check(VertexFormatSize(VkNum::kFormatR16G16Sfloat, size) && size == 4,
          "R16G16_SFLOAT is 4 bytes");
    Check(VertexFormatSize(VkNum::kFormatR32G32Sfloat, size) && size == 8,
          "R32G32_SFLOAT is 8 bytes");
    Check(VertexFormatSize(VkNum::kFormatR32G32B32Sfloat, size) && size == 12,
          "R32G32B32_SFLOAT is 12 bytes");
    Check(VertexFormatSize(VkNum::kFormatR32G32B32A32Sfloat, size) &&
              size == 16,
          "R32G32B32A32_SFLOAT is 16 bytes");
    Check(!VertexFormatSize(VkNum::kFormatD32Sfloat, size),
          "D32_SFLOAT is NOT a vertex format");
    Check(!VertexFormatSize(VkNum::kFormatUndefined, size),
          "UNDEFINED is NOT a vertex format");
    Check(IsDepthFormat(VkNum::kFormatD16Unorm) &&
              IsDepthFormat(VkNum::kFormatD32Sfloat) &&
              IsDepthFormat(VkNum::kFormatD24UnormS8Uint) &&
              IsDepthFormat(VkNum::kFormatD32SfloatS8Uint) &&
              !IsDepthFormat(VkNum::kFormatR8G8B8A8Unorm) &&
              !IsDepthFormat(VkNum::kFormatUndefined),
          "depth-format predicate");

    // ---- 2. merge semantics -------------------------------------------------
    {
        GuestShaderReflection vs = Reflect(
            {{7, 0, 0, 4, true, 0}, {4, 0, 1, 1, true, 0}}, 32u);
        GuestShaderReflection fs = Reflect(
            {{7, 0, 0, 4, false, 0}, {2, 0, 2, 1, true, 0}}, 48u);
        MergedGraphicsReflection m;
        std::string err;
        const bool ok = MergeGraphicsReflections(vs, fs, m, err);
        Check(ok && m.bindings.size() == 3, "merge: union of three bindings");
        Check(ok && m.bindings[0].set == 0 && m.bindings[0].binding == 0 &&
                  m.bindings[1].binding == 1 && m.bindings[2].binding == 2,
              "merge: output sorted by (set, binding)");
        Check(ok &&
                  m.bindings[0].stageFlags ==
                      (VkNum::kStageVertexBit | VkNum::kStageFragmentBit) &&
                  m.bindings[1].stageFlags == VkNum::kStageVertexBit &&
                  m.bindings[2].stageFlags == VkNum::kStageFragmentBit,
              "merge: stage masks OR across stages");
        Check(ok && m.bindings[0].readOnly == false &&
                  m.bindings[1].readOnly == true,
              "merge: readOnly ANDs (either writing => writable)");
        Check(ok && m.pushConstantSizeBytes == 48 &&
                  m.pushConstantStageFlags ==
                      (VkNum::kStageVertexBit | VkNum::kStageFragmentBit),
              "merge: push window is max size + OR of declaring stages");
    }
    {
        GuestShaderReflection vs =
            Reflect({{7, 0, 0, 4, true, 0}}, 0u);
        GuestShaderReflection fs =
            Reflect({{4, 0, 0, 4, true, 0}}, 0u);  // same slot, other type
        MergedGraphicsReflection m;
        std::string err;
        const bool ok = MergeGraphicsReflections(vs, fs, m, err);
        Check(!ok &&
                  err.find("set 0 binding 0") != std::string::npos &&
                  err.find("type") != std::string::npos,
              "merge: type conflict refuses naming the (set, binding)");
    }
    {
        GuestShaderReflection vs =
            Reflect({{7, 0, 0, 4, true, 0}}, 0u);
        GuestShaderReflection fs =
            Reflect({{7, 0, 0, 8, true, 0}}, 0u);  // same type, other count
        MergedGraphicsReflection m;
        std::string err;
        const bool ok = MergeGraphicsReflections(vs, fs, m, err);
        Check(!ok && err.find("count") != std::string::npos,
              "merge: count conflict refuses naming the count");
    }
    {
        // Empty reflections on both stages merge to an empty table — real
        // reflection of a shader pair that reads nothing.
        MergedGraphicsReflection m;
        std::string err;
        const bool ok = MergeGraphicsReflections(Reflect({}, 0u),
                                                 Reflect({}, 0u), m, err);
        Check(ok && m.bindings.empty() &&
                  m.pushConstantSizeBytes == 0u &&
                  m.pushConstantStageFlags == 0u,
              "merge: empty stages merge to an empty table");
    }

    // ---- 3. happy path: the desc a 2D quad draw would build ------------------
    {
        GraphicsPipelineDesc d;
        d.topology = VkNum::kTopologyTriangleStrip;
        d.vertexBindings = {{0, 20, false}};
        d.vertexAttributes = {
            {0, 0, VkNum::kFormatR32G32Sfloat, 0},    // position
            {1, 0, VkNum::kFormatR32G32Sfloat, 8},    // uv
            {2, 0, VkNum::kFormatR8G8B8A8Unorm, 16},  // color
        };
        d.colorTargetCount = 1;
        d.colorFormats[0] = VkNum::kFormatR8G8B8A8Unorm;
        d.blends = {GraphicsTargetBlend{}};
        d.depthFormat = 0;
        std::string err;
        Check(ValidateGraphicsPipelineDesc(d, err),
              "happy path: textured-quad desc validates");
    }

    // ---- 4. named rejections --------------------------------------------------
    const auto base = [] {
        GraphicsPipelineDesc d;
        d.vertexBindings = {{0, 8, false}};
        d.vertexAttributes = {{0, 0, VkNum::kFormatR32G32Sfloat, 0}};
        d.colorFormats[0] = VkNum::kFormatR8G8B8A8Unorm;
        d.blends = {GraphicsTargetBlend{}};  // match the default 1 target
        return d;
    };

    { auto d = base(); d.topology = 6;
      CheckRejected(d, "topology", "rej: topology out of range"); }
    { auto d = base(); d.colorTargetCount = 0;
      CheckRejected(d, "count is zero", "rej: zero color targets"); }
    { auto d = base(); d.colorTargetCount = 9;
      CheckRejected(d, "exceeds the 8 attachment limit",
                    "rej: too many color targets"); }
    { auto d = base(); d.blends = {{}, {}};
      CheckRejected(d, "blend state count 2 does not match the 1 color targets",
                    "rej: blend/target count mismatch"); }
    { auto d = base(); d.colorFormats[0] = 0;
      CheckRejected(d, "UNDEFINED", "rej: undefined color format"); }
    { auto d = base(); d.colorTargetCount = 2;
      d.colorFormats[0] = VkNum::kFormatR8G8B8A8Unorm;
      d.colorFormats[1] = VkNum::kFormatR8G8B8A8Unorm;
      d.blends = {{}, {}};
      CheckRejected(d, "same format 37", "rej: duplicate color format"); }
    { auto d = base(); d.depthFormat = VkNum::kFormatR32Sfloat;
      CheckRejected(d, "not one of D16/D32/D24S8/D32S8",
                    "rej: non-depth format as depth attachment"); }
    { auto d = base(); d.depthFormat = VkNum::kFormatD32Sfloat;
      d.colorFormats[0] = VkNum::kFormatD32Sfloat;
      CheckRejected(d, "used as color target",
                    "rej: depth format as color target"); }
    { auto d = base(); d.vertexBindings = {{0, 8, false}, {0, 16, false}};
      CheckRejected(d, "declared twice", "rej: duplicate vertex binding"); }
    { auto d = base(); d.vertexAttributes = {{0, 3, VkNum::kFormatR32G32Sfloat, 0}};
      CheckRejected(d, "references undeclared binding 3",
                    "rej: attribute references undeclared binding"); }
    { auto d = base();
      d.vertexAttributes = {{0, 0, VkNum::kFormatD16Unorm, 0}};
      CheckRejected(d, "not in the vertex format allowlist",
                    "rej: depth format as vertex attribute format"); }
    { auto d = base(); d.vertexBindings = {{0, 0, false}};
      CheckRejected(d, "stride 0", "rej: zero stride on used binding"); }
    { auto d = base();
      d.vertexAttributes = {{0, 0, VkNum::kFormatR32G32B32A32Sfloat, 8}};
      CheckRejected(d, "overruns binding 0 stride 8",
                    "rej: attribute overruns stride"); }
    { auto d = base();
      d.depthStencil.depthWriteEnable = true;
      CheckRejected(d, "depthWriteEnable requires depthTestEnable",
                    "rej: depth write without depth test"); }
    { auto d = base(); d.dynamicViewportScissor = false;
      CheckRejected(d, "dynamic only",
                    "rej: static viewport/scissor refused"); }
    { auto d = base(); d.blends[0].srcColorFactor = 19;
      CheckRejected(d, "0..18", "rej: blend factor out of range"); }
    { auto d = base(); d.depthStencil.depthCompareOp = 8;
      CheckRejected(d, "0..7", "rej: compare op out of range"); }

    if (g_failures == 0) {
        std::printf("graphics_pipeline_desc_test: ALL PASS\n");
        return 0;
    }
    std::printf("graphics_pipeline_desc_test: %d FAILURES\n", g_failures);
    return 1;
}
