// PX5 v1.47 — see nid.h for the contract.
//
// SHA-1 and the NID string derivation are ported from AnyPS5
// core/libs/nid/src/{Sha1.cpp,NidCompute.cpp} (GPL-2.0-only, pinned at
// c72d5ce1). The port keeps the upstream arithmetic byte-for-byte so the
// produced NID strings match the real toolchain bit-for-bit; the host gate
// pins golden vectors computed independently of this translation unit.
#include "nid.h"

#include <array>
#include <cstdint>
#include <vector>

namespace PX5::Nid {

namespace {

constexpr uint8_t kNidSuffix[16] = {
    0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30
};

constexpr char kBase64S[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

// SHA-1 over a byte span (ported from AnyPS5 core/libs/nid/src/Sha1.cpp).
std::array<uint8_t, 20> Sha1(const uint8_t* data, size_t len) {
    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xEFCDAB89u;
    uint32_t h2 = 0x98BADCFEu;
    uint32_t h3 = 0x10325476u;
    uint32_t h4 = 0xC3D2E1F0u;

    std::vector<uint8_t> msg(data, data + len);
    const uint64_t bitLen = static_cast<uint64_t>(len) * 8u;

    msg.push_back(0x80u);
    while ((msg.size() % 64u) != 56u)
        msg.push_back(0x00u);
    for (int i = 7; i >= 0; --i)
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8u)) & 0xffu));

    const auto rotl32 = [](uint32_t x, unsigned n) -> uint32_t {
        return (x << n) | (x >> (32u - n));
    };

    for (size_t offset = 0; offset < msg.size(); offset += 64u) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[offset + i * 4u + 0u]) << 24u)
                 | (static_cast<uint32_t>(msg[offset + i * 4u + 1u]) << 16u)
                 | (static_cast<uint32_t>(msg[offset + i * 4u + 2u]) <<  8u)
                 | (static_cast<uint32_t>(msg[offset + i * 4u + 3u]));
        }
        for (int i = 16; i < 80; ++i)
            w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1u);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const uint32_t temp = rotl32(a, 5u) + f + e + k + w[i];
            e = d; d = c; c = rotl32(b, 30u); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<uint8_t, 20> digest{};
    const auto put = [&](int base, uint32_t v) {
        digest[base + 0] = static_cast<uint8_t>(v >> 24u);
        digest[base + 1] = static_cast<uint8_t>(v >> 16u);
        digest[base + 2] = static_cast<uint8_t>(v >>  8u);
        digest[base + 3] = static_cast<uint8_t>(v);
    };
    put(0, h0); put(4, h1); put(8, h2); put(12, h3); put(16, h4);
    return digest;
}

bool InBase64S(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '-';
}

} // namespace

std::string ComputeNidString(const std::string& symbolName) {
    if (symbolName.empty()) return {};

    std::vector<uint8_t> input;
    input.reserve(symbolName.size() + sizeof(kNidSuffix));
    for (char c : symbolName)
        input.push_back(static_cast<uint8_t>(c));
    for (uint8_t b : kNidSuffix)
        input.push_back(b);

    const auto digest = Sha1(input.data(), input.size());

    uint8_t reversed[8];
    for (int i = 0; i < 8; ++i)
        reversed[i] = digest[7 - i];

    // 8 bytes -> 11 base64S characters (5 full triples + a 2-byte tail).
    std::string nid;
    nid.reserve(11);
    for (int i = 0; i < 6; i += 3) {
        const uint32_t triple =
            (static_cast<uint32_t>(reversed[i]) << 16) |
            (static_cast<uint32_t>(reversed[i + 1]) << 8) |
            static_cast<uint32_t>(reversed[i + 2]);
        nid.push_back(kBase64S[(triple >> 18) & 0x3F]);
        nid.push_back(kBase64S[(triple >> 12) & 0x3F]);
        nid.push_back(kBase64S[(triple >> 6) & 0x3F]);
        nid.push_back(kBase64S[triple & 0x3F]);
    }
    const uint32_t tail =
        (static_cast<uint32_t>(reversed[6]) << 16) |
        (static_cast<uint32_t>(reversed[7]) << 8);
    nid.push_back(kBase64S[(tail >> 18) & 0x3F]);
    nid.push_back(kBase64S[(tail >> 12) & 0x3F]);
    nid.push_back(kBase64S[(tail >> 6) & 0x3F]);
    return nid;   // exactly 11 characters
}

bool IsNidString(const std::string& s) {
    if (s.size() != 11) return false;
    for (char c : s)
        if (!InBase64S(c)) return false;
    return true;
}

NidRegistry& NidRegistry::Instance() {
    static NidRegistry inst;
    return inst;
}

void NidRegistry::RegisterSymbol(const std::string& symbolName) {
    if (symbolName.empty()) return;
    const std::string nid = ComputeNidString(symbolName);
    if (nid.empty()) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    m_nidToSymbol.emplace(nid, symbolName);   // first registration wins
}

std::string NidRegistry::Resolve(const std::string& nidString) const {
    if (!IsNidString(nidString)) return {};
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_nidToSymbol.find(nidString);
    return it == m_nidToSymbol.end() ? std::string{} : it->second;
}

size_t NidRegistry::Size() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_nidToSymbol.size();
}

} // namespace PX5::Nid
