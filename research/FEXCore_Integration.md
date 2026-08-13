# FEXCore Integration Research

## CPU Emulation Architecture
FEXCore serves as the x86-64 to ARM64 binary translator for PX5. It is integrated as a native library loaded via JNI (`FexCoreWrapper`). 

## Constraints
As defined in `AGENTS.md` (The 15 Laws):
- **Law 2 & 4**: ARM64 Only. FEXCore is the *only* CPU backend. We do not write a custom recompiler.
- **Law 3**: Bionic Runtime Only. We must not rely on `glibc`, Debian containers, or `proot`. FEXCore must be adapted to work directly under Android's Bionic libc.

## Next Steps
1. Initialize FEXCore state within `fexcore_wrapper.cpp`.
2. Map PS5 ELF/SELF memory regions using Android's memory allocator (handling `ashmem` deprecation gracefully).
3. Ensure the JNI layer minimizes overhead when interacting with the Kotlin UI.
