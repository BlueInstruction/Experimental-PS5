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
| `FEXCore/include/FEXCore/Utils/SpinWaitLock.h`, `FEXCore/include/FEXCore/Utils/WritePriorityMutex.h` (and any other consumer of `std::atomic_ref<T>`) | Bionic's libc++ (NDK 26 and NDK 27) does not export `std::atomic_ref<T>` in `<atomic>` even with `-std=c++20` | C++20 feature-test macro `__cpp_lib_atomic_ref` is not defined | Force-include `app/src/main/cpp/compat/std_atomic_ref_polyfill.h` in every C++ translation unit (px5 + FEX) via `add_compile_options(-include ...)` in `app/src/main/cpp/CMakeLists.txt`. The polyfill uses `__atomic_*` GCC/Clang builtins and is a no-op if a future NDK ships libc++ with `std::atomic_ref` enabled. | Yes |

## C++ standard

`std::atomic_ref` is a C++20 feature. FEX's own `CMakeLists.txt` sets
`CMAKE_CXX_STANDARD=20` only at its subdirectory scope, which does not
propagate up to the parent `CMakeLists.txt` where the `px5` target is
defined. The parent (`app/src/main/cpp/CMakeLists.txt`) sets it at parent
scope BEFORE `add_subdirectory(fex)` so it propagates DOWN to FEX as well,
and additionally calls `target_compile_features(px5 PRIVATE cxx_std_20)`
as belt-and-suspenders.

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
