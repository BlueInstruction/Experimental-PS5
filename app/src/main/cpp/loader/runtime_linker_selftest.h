// SPDX-License-Identifier: MIT
// PX5 v1.31 — runtime linker self-test + the NID-gate guest stub.
//
// Two layers live here:
//   * RunRuntimeLinkerSelfTest  — pure C++ (registry + PT_DYNAMIC reader
//     over synthetic in-memory ELF images). No engine, no JNI: it runs
//     for real on BOTH ABIs (CI smoke + on-device loader self-test).
//   * kPx5NidGateStubCode       — hand-assembled x86-64 guest bytes used
//     by the foundation self-test step 10 (guest -> gate syscall ->
//     bionic HLE -> exit 42). Encodings verified instruction by
//     instruction in the comment next to each byte group.

#ifndef PX5_LOADER_RUNTIME_LINKER_SELFTEST_H
#define PX5_LOADER_RUNTIME_LINKER_SELFTEST_H

#include <cstdint>
#include <string>

namespace PX5 {

// Runs all subtests; returns true iff every one passed. `report` (optional)
// receives a multi-line report whose first line begins PASS or FAIL.
bool RunRuntimeLinkerSelfTest(std::string* report);

// --- NID-gate guest fixture ------------------------------------------------
// Synthetic-but-clearly-ours NID: the ASCII of "PX51" (0x50 0x58 0x35 0x31).
constexpr uint64_t kGateTestNid        = 0x50583531ull;
constexpr uint64_t kGateExpectedExit   = 42;   // HLE returns 7+35; guest exits it

// x86-64 guest stub:
//   mov rdi, kGateTestNid        ; 48 C7 C7 <imm32>  — gate a0 = NID
//   mov rsi, 7                   ; 48 C7 C6 <imm32>  — gate a1 (HLE arg0)
//   mov rdx, 35                  ; 48 C7 C2 <imm32>  — gate a2 (HLE arg1)
//   mov eax, 0x5C500001          ; B8 <imm32>        — reserved gate number
//   syscall                      ; 0F 05             — into GuestSyscalls
//   mov rdi, rax                 ; 48 89 C7          — exit code = HLE result
//   mov eax, 231                 ; B8 <imm32>        — exit_group
//   syscall                      ; 0F 05
//   hlt                          ; F4                — clean JIT stop
//
// v1.32 — TWO device-proven fixes to this fixture (vc32 session log):
//   1. The immediate was hand-assembled as 01 00 C5 5C = 0x5CC50001 —
//      NOT the gate number. The device log shows exactly that: "SYSCALL
//      1556414465 (=0x5CC50001): unknown -> ENOSYS" then
//      "exit_group(18446744073709551578)" (= -38). Byte 3 is 0x50.
//   2. A trailing HLT is MANDATORY: GuestSyscalls records exit_group but
//      the JIT block keeps executing after the syscall returns (the HLT
//      is the only clean unwinding convention — EnableExitOnHLT). The
//      old stub ENDED at the exit syscall, so the JIT ran off the stub
//      into the zeroed page tail: x86 "00 00" = add byte [rax], al with
//      rax=0 -> SIGSEGV si_addr=0x0 in the JIT region — the vc32
//      crash #1 that killed the whole in-process foundation suite.
constexpr uint8_t kNidGateStubCode[] = {
    0x48, 0xC7, 0xC7, 0x31, 0x35, 0x58, 0x50,   // mov rdi, 0x50583531
    0x48, 0xC7, 0xC6, 0x07, 0x00, 0x00, 0x00,   // mov rsi, 7
    0x48, 0xC7, 0xC2, 0x23, 0x00, 0x00, 0x00,   // mov rdx, 35
    0xB8, 0x01, 0x00, 0x50, 0x5C,               // mov eax, 0x5C500001 (v1.32 byte fix)
    0x0F, 0x05,                                 // syscall (gate)
    0x48, 0x89, 0xC7,                           // mov rdi, rax
    0xB8, 0xE7, 0x00, 0x00, 0x00,               // mov eax, 231
    0x0F, 0x05,                                 // syscall (exit_group)
    0xF4,                                       // hlt (clean JIT stop; unreachable when exit works)
};
constexpr size_t kNidGateStubSize = sizeof(kNidGateStubCode);

} // namespace PX5

#endif // PX5_LOADER_RUNTIME_LINKER_SELFTEST_H
