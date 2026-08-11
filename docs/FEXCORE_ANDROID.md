# FEXCore Android Integration

## Upstream source

The vendored source is FEX commit `fd141ed6d721d03062619e4702bca1a0c93b6dd9`.
It is stored under `third_party/fex` with its upstream CMake structure and
required submodules.

## Android-specific changes

| Upstream file | Reason | Android limitation | Patch | Android-specific |
| --- | --- | --- | --- | --- |
| `CMakeLists.txt` | FEX rejects Android as a supported system | Android uses Bionic and reports `CMAKE_SYSTEM_NAME=Android` | Allow Android in the platform check | Yes |
| `CMakeLists.txt` | FEX defaults to the glibc jemalloc hook | Bionic does not provide the glibc allocator ABI | Set `ENABLE_JEMALLOC_GLIBC_ALLOC=OFF` from the app build | Yes |
| `FEXCore/Source/Utils/AllocatorHooks.cpp` | Bionic does not provide the glibc `valloc()` ABI used by the fallback allocator | The fallback allocator must still provide page-aligned memory | Use Bionic's `memalign()` with the runtime page size | Yes |

No `std::atomic_ref` compatibility patch is applied yet; it must only be added
after an Android build demonstrates the corresponding failure and the exact
patch is recorded here.

## CMake dependency graph

The app links the real targets with:

```cmake
target_link_libraries(px5
        FEXCore
        FEXCore_Base
        JemallocDummy)
```

FEXCore's own CMake supplies its required upstream dependencies. No local
FEXCore replacement target is created.

## Runtime status

Runtime initialization and guest execution require a physical Android ARM64
device. This environment has no connected ADB device, so no runtime result is
claimed until logcat evidence is captured.