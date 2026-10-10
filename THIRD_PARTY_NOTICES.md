# Third-Party Notices

Code vendored into this repository that PX5 did not write. Each entry lists
origin, revision, license and scope. License compatibility is GPL-2.0-only
(the project license, `LICENSE`).

## Shader recompiler

- **Origin:** AnyPS5 — `core/shader/recompiler/`
  (https://github.com/BlueInstruction/AnyPS5)
- **Vendored revision:** `262c031692a0ce538707436fa1587b671ace682b` (2026-10-11)
- **License:** GPL-2.0-only (text preserved at
  `app/src/main/cpp/gpu/shader_recompiler/LICENSE.AnyPS5`)
- **Location:** `app/src/main/cpp/gpu/shader_recompiler/`
- **Scope:** RDNA (GFX10.3) shader decode → IR → control-flow structuring →
  translation → SPIR-V emission → compiled-shader disk cache.
- **Adaptations:** limited and enumerated in
  `app/src/main/cpp/gpu/shader_recompiler/PX5_PORT.md`.

## SPIRV-Headers

- **Origin:** KhronosGroup/SPIRV-Headers
  (https://github.com/KhronosGroup/SPIRV-Headers)
- **Vendored revision:** `86f980c731e62ae4eaf383d320449d71687936bf` (2026-10-02)
- **License:** MIT-style Khronos license (text preserved at
  `app/src/main/cpp/third_party/spirv-headers/LICENSE`)
- **Location:** `app/src/main/cpp/third_party/spirv-headers/include`
- **Scope:** header-only SPIR-V Unified1 definitions consumed by the
  recompiler's SPIR-V backend.
