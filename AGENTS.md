# PX5 Development Constitution

> **This document is the supreme law of the PX5 project.**
> Every contributor — human or AI agent — must read, understand, and obey these laws.

## The 15 Laws

1. **Android Only** — No iOS, no desktop, no embedded Linux.
2. **ARM64 Only** — No x86, no 32-bit ARM.
3. **Bionic Runtime Only** — No glibc. No Debian container. No proot/chroot.
4. **FEXCore is the Only CPU Backend** — Never write a custom x86-64 recompiler.
5. **Vulkan Only** — No OpenGL. No DirectX. No Metal.
6. **No OpenGL Renderer** — If Vulkan is unavailable, PX5 does not run.
7. **No DirectX Backend** — GNM/GNMX → Vulkan only.
8. **No glibc Frontend** — No X11, no ALSA, no SDL. Android Surface + AAudio directly.
9. **Never Bundle Mesa Inside APK** — Turnip is system-level, never in APK.
10. **Every Feature Must Have Tests** — No untested code accepted.
11. **Every Subsystem Has a Specification** — Code contradicting its spec is a bug.
12. **Every PR Must Preserve Android Compatibility** — No desktop-only code.
13. **Performance Regressions Are Not Accepted** — >2% FPS drop needs justification.
14. **Follow Clean Architecture** — No circular dependencies. Clear public APIs.
15. **No Platform-Specific Hacks Unless Documented** — Every hack needs a comment + flag + removal path.

## AI Agent Rules (CRITICAL)

If you are an AI agent working on PX5:

1. **NEVER commit secrets** — No tokens, passwords, API keys, or .env files.
2. **NEVER copy another project** — Don't dump VKD3D, Turnip, or any other repo into PX5.
3. **NEVER modify files outside your task scope** — Only touch what's needed.
4. **NEVER create stubs unless explicitly asked** — Real implementations only.
5. **NEVER use third-party code without checking license compatibility** — Document sources.
6. **ALWAYS work on a branch** — No direct commits to main for major changes.
7. **ALWAYS read AGENTS.md first** — Understand the laws before writing code.
8. **ALWAYS read the relevant .agents/ skill** — Each subsystem has guidance.
9. **ALWAYS read specifications/** — Understand the design before implementing.
10. **ALWAYS check roadmap/** — Understand current phase and priorities.

## Architecture

```
                    PX5
          Android Application (Kotlin)
                    │
         ┌──────────┴──────────┐
         │    Bionic Runtime    │
         │    FEXCore Engine    │  ← x86-64 → ARM64 (Law 4)
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │   Kernel HLE        │
         │   ELF/SELF Loader   │
         │   Memory Manager    │
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │   PS5 Libraries     │
         │   (AGC, AJM, etc.)  │
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │   GNM / GNMX        │  → PSSL → SPIR-V
         │   Vulkan Renderer   │  → Turnip / Adreno
         │   Android Surface   │  (Law 5, 8)
         └─────────────────────┘
```

## License

GPL-3.0-or-later
