#pragma once

#include <cstdint>
#include <string>

/**
 * @file LevelShare.h
 * @brief Stable share codes and portable level sharing for UGC (issue #241).
 *
 * Milestone 17, "Doodlebound" Phase 5. Turns a level (the doodle-level JSON,
 * game::toLevelJson) into something friends can pass around:
 *
 *   - shareCodeFor(json): a stable, content-addressed code. The same level yields
 *     the same code on any device (it hashes the JSON), unlike the catalog's
 *     per-instance sequential codes, so it is a durable identity for a level.
 *   - encodeShare(json) / decodeShare(str): a self-contained share string that
 *     carries the whole level, so it re-imports byte-identical on another instance
 *     with no shared backend. The string embeds the content code; decodeShare
 *     recomputes it from the decoded payload and rejects any mismatch, so tampered
 *     or truncated shares fail instead of importing a wrong level.
 *
 * The payload is the existing JSON (base64url of the exact bytes) - no new bespoke
 * level serialization, per the issue. Pure std, header-only, no wall clock.
 */
namespace IKore {
namespace game {
namespace detail {

/// FNV-1a 64-bit hash of a byte string (stable across platforms).
inline std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

/// Encode @p v as @p chars Crockford base32 digits (excludes I, L, O, U).
inline std::string base32Crockford(std::uint64_t v, int chars) {
    static const char* kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    std::string s(static_cast<std::size_t>(chars), '0');
    for (int i = chars - 1; i >= 0; --i) {
        s[static_cast<std::size_t>(i)] = kAlphabet[v & 31u];
        v >>= 5;
    }
    return s;
}

inline const std::string& base64urlAlphabet() {
    static const std::string a =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    return a;
}

/// base64url encode (URL-safe alphabet, no padding).
inline std::string base64Encode(const std::string& in) {
    const std::string& A = base64urlAlphabet();
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    std::size_t i = 0;
    while (i + 3 <= in.size()) {
        const std::uint32_t n = (static_cast<std::uint8_t>(in[i]) << 16) |
                                (static_cast<std::uint8_t>(in[i + 1]) << 8) |
                                static_cast<std::uint8_t>(in[i + 2]);
        out += A[(n >> 18) & 63];
        out += A[(n >> 12) & 63];
        out += A[(n >> 6) & 63];
        out += A[n & 63];
        i += 3;
    }
    const std::size_t rem = in.size() - i;
    if (rem == 1) {
        const std::uint32_t n = static_cast<std::uint8_t>(in[i]) << 16;
        out += A[(n >> 18) & 63];
        out += A[(n >> 12) & 63];
    } else if (rem == 2) {
        const std::uint32_t n = (static_cast<std::uint8_t>(in[i]) << 16) |
                                (static_cast<std::uint8_t>(in[i + 1]) << 8);
        out += A[(n >> 18) & 63];
        out += A[(n >> 12) & 63];
        out += A[(n >> 6) & 63];
    }
    return out;
}

/// base64url decode. Returns false on any invalid character or bad length.
inline bool base64Decode(const std::string& in, std::string& out) {
    int rev[256];
    for (int i = 0; i < 256; ++i) rev[i] = -1;
    const std::string& A = base64urlAlphabet();
    for (std::size_t i = 0; i < A.size(); ++i) rev[static_cast<unsigned char>(A[i])] = static_cast<int>(i);

    out.clear();
    if (in.size() % 4 == 1) return false; // never a valid base64url length
    std::uint32_t buf = 0;
    int bits = 0;
    for (char ch : in) {
        const int v = rev[static_cast<unsigned char>(ch)];
        if (v < 0) return false;
        buf = (buf << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((buf >> bits) & 0xFF);
        }
    }
    return true;
}

} // namespace detail

/// A stable, content-addressed share code for a level's JSON (same input -> same
/// code on any device). Format: "DD-" followed by 13 Crockford base32 digits.
/// ("DD-" and the "DDL1:" prefix below predate the Doodlebound rename; they are
/// frozen wire-format identifiers and must not change with branding.)
inline std::string shareCodeFor(const std::string& levelJson) {
    return "DD-" + detail::base32Crockford(detail::fnv1a64(levelJson), 13);
}

/// Build a self-contained share string that carries the level so it can be
/// re-imported byte-identical elsewhere. Format: "DDL1:<code>:<base64url(json)>".
inline std::string encodeShare(const std::string& levelJson) {
    return "DDL1:" + shareCodeFor(levelJson) + ":" + detail::base64Encode(levelJson);
}

/// Result of decoding a share string.
struct ShareImport {
    bool ok{false};
    std::string code;      ///< the content code carried by the share.
    std::string levelJson; ///< the recovered level JSON (byte-identical when ok).
};

/**
 * @brief Decode a share string produced by encodeShare().
 *
 * Validates the format, base64url-decodes the payload, and verifies that the
 * content code recomputed from the decoded JSON matches the code in the string.
 * A tampered payload, wrong code, bad base64, or wrong prefix yields ok == false.
 */
inline ShareImport decodeShare(const std::string& share) {
    ShareImport r;
    const std::string prefix = "DDL1:";
    if (share.size() <= prefix.size() || share.compare(0, prefix.size(), prefix) != 0) return r;
    const std::size_t codeStart = prefix.size();
    const std::size_t sep = share.find(':', codeStart);
    if (sep == std::string::npos) return r;
    const std::string code = share.substr(codeStart, sep - codeStart);
    const std::string payload = share.substr(sep + 1);
    std::string json;
    if (!detail::base64Decode(payload, json)) return r;
    if (shareCodeFor(json) != code) return r; // integrity: code must match content
    r.ok = true;
    r.code = code;
    r.levelJson = std::move(json);
    return r;
}

} // namespace game
} // namespace IKore
