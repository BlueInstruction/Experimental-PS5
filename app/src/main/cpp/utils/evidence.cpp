// SPDX-License-Identifier: MIT
// PX5 — Evidence layer implementation (v1.41). See evidence.h for the why.
#include "evidence.h"

#include "../utils/logger.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

namespace PX5 {
namespace Evidence {

// ---------------------------------------------------------------------------
// SHA-256 (FIPS 180-4) — small, dependency-free, allocation-free.
// ---------------------------------------------------------------------------
namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t Rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

void Compress(uint32_t h[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t)block[i * 4] << 24 | (uint32_t)block[i * 4 + 1] << 16 |
               (uint32_t)block[i * 4 + 2] << 8 | (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^
                            (w[i - 15] >> 3);
        const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^
                            (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        const uint32_t S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void Sha256Raw(const void* data, size_t len, uint8_t digest[32]) {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const uint8_t* p = static_cast<const uint8_t*>(data);
    const size_t full = len / 64;
    for (size_t i = 0; i < full; ++i) Compress(h, p + i * 64);

    uint8_t tail[128];
    const size_t rem = len - full * 64;
    memcpy(tail, p + full * 64, rem);
    tail[rem] = 0x80;
    const size_t padded = (rem + 1 <= 56) ? 64 : 128;
    memset(tail + rem + 1, 0, padded - rem - 1 - 8);
    const uint64_t bits = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i)
        tail[padded - 1 - i] = static_cast<uint8_t>(bits >> (8 * i));
    Compress(h, tail);
    if (padded == 128) Compress(h, tail + 64);

    for (int i = 0; i < 8; ++i) {
        digest[i * 4]     = static_cast<uint8_t>(h[i] >> 24);
        digest[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
        digest[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
        digest[i * 4 + 3] = static_cast<uint8_t>(h[i]);
    }
}

std::mutex g_ledgerMutex;
std::string g_ledgerPath;

ImageIdentity g_image;                 // BSS — zero-initialized
bool g_sessionReal = false;            // log-namespace flag (default SYNTH)
int g_syscallNotes = 0;                // capped ledger budget for syscalls

constexpr int kMaxSyscallNotes = 16;

} // namespace

void Sha256(const void* data, size_t len, uint8_t digest[32]) {
    Sha256Raw(data, len, digest);
}

void Sha256Hex(const void* data, size_t len, char out[65]) {
    uint8_t d[32];
    Sha256Raw(data, len, d);
    static const char* hex = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[i * 2]     = hex[d[i] >> 4];
        out[i * 2 + 1] = hex[d[i] & 0xf];
    }
    out[64] = '\0';
}

bool SelfTest() {
    char h[65];
    // "" -> e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    Sha256Hex("", 0, h);
    if (strcmp(h, "e3b0c44298fc1c149afbf4c8996fb924"
                   "27ae41e4649b934ca495991b7852b855") != 0) return false;
    // "abc" -> ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    Sha256Hex("abc", 3, h);
    if (strcmp(h, "ba7816bf8f01cfea414140de5dae2223"
                   "b00361a396177a9cb410ff61f20015ad") != 0) return false;
    // 56-byte vector (crosses the 55-byte pad boundary):
    // "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" is 56.
    const char* two =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    Sha256Hex(two, strlen(two), h);
    if (strcmp(h, "248d6a61d20638b8e5c026930c3e6039"
                   "a33ce45964ff2167f6ecedd419db06c1") != 0) return false;
    // 64-byte block-aligned vector — exercises the second Compress call.
    uint8_t block[64];
    memset(block, 'a', 64);
    Sha256Hex(block, 64, h);
    if (strcmp(h, "ffe054fe7ae0cb6dc65c3af9b61d5209"
                   "f439851db43d0ba5997337df154668eb") != 0) return false;
    return true;
}

void BindImage(const ImageIdentity& img) {
    g_image = img;   // plain BSS copy; set once before any dispatch
    g_syscallNotes = 0;   // fresh ledger budget per bound image
    // The ledger line is the anchor event every later line refers back to.
    AppendLedger("image bound stream=%s sha256=%s container_sha256=%s "
                 "size=%llu entry=0x%llx self=%d segs=%d path=%s",
                 img.stream == Stream::InnerElf ? "inner_elf" : "file",
                 img.sha256, img.containerSha256,
                 (unsigned long long)img.streamSize,
                 (unsigned long long)img.entry, img.isSelf ? 1 : 0,
                 img.segCount, img.path);
}

bool GetImage(ImageIdentity& out) {
    if (!g_image.valid) return false;
    out = g_image;
    return true;
}

void SetSessionRealGuest(bool real) {
    g_sessionReal = real;
    // Namespace switches are ledger events too — a reviewer sees exactly
    // where the synthetic suite ended and the real-guest path began.
    AppendLedger("session namespace -> %s", real ? "REAL-GUEST" : "SYNTH");
}

bool SessionIsRealGuest() { return g_sessionReal; }

void SetLedgerPath(const std::string& path) {
    std::lock_guard<std::mutex> lk(g_ledgerMutex);
    g_ledgerPath = path;
}

void AppendLedger(const char* format, ...) {
    std::lock_guard<std::mutex> lk(g_ledgerMutex);
    if (g_ledgerPath.empty()) return;

    // One line: <epoch_ms> <event...> [img=sha256-prefix]
    char body[512];
    va_list ap;
    va_start(ap, format);
    vsnprintf(body, sizeof(body), format, ap);
    va_end(ap);

    char line[640];
    if (g_image.valid) {
        snprintf(line, sizeof(line), "%lld %.8s %s\n",
                 (long long)(time(nullptr)), g_image.sha256, body);
    } else {
        snprintf(line, sizeof(line), "%lld - %s\n",
                 (long long)(time(nullptr)), body);
    }

    const int fd = open(g_ledgerPath.c_str(),
                        O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    const ssize_t n = write(fd, line, strlen(line));
    (void)n;
    fsync(fd);
    close(fd);
}

void NoteGuestSyscall(uint32_t nr, uint64_t a0, uint64_t a1) {
    if (g_syscallNotes >= kMaxSyscallNotes) return;
    ++g_syscallNotes;
    AppendLedger("guest syscall #%d nr=%u a0=0x%llx a1=0x%llx",
                 g_syscallNotes, nr, (unsigned long long)a0,
                 (unsigned long long)a1);
    // Mirror into the main log under the [GUEST] namespace (capped too).
    PX5_LOGI(LogCategory::KERNEL,
             "[GUEST] syscall nr=%u a0=0x%llx a1=0x%llx (evidence %d/%d)",
             nr, (unsigned long long)a0, (unsigned long long)a1,
             g_syscallNotes, kMaxSyscallNotes);
}

} // namespace Evidence
} // namespace PX5
