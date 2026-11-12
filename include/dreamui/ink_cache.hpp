#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace dreamui {
// The ONLY memory access is through this injected read-only callback.
// Tests use mock memory; Windows provides guarded ReadProcessMemory.
using MemoryRead = std::function<bool(std::uint64_t,void*,std::size_t)>;
struct InkCacheProbe {
    enum class Status {
        Invalid, EmptyCache, NotCached, ExpiredToken, TokenPending,
        TokenFailed, StyleAvailable, PropertyCandidate
    } status = Status::Invalid;
    std::string reason;
    std::uint32_t cached_tokens=0, styles=0, properties=0;
    bool found_token=false, found_property=false;
    // Candidate floats are not authenticated as HDRColor until RTTI type is
    // verified. They must never be applied to a game resource.
    float rgba[4]{};
};
std::uint64_t resource_path_hash(std::string_view path);
std::uint64_t cname_hash(std::string_view name);
// Inspects an ALREADY-CACHED ResourceLoader token without calling native
// functions, creating resource requests, taking locks or writing to memory.
// Unknown versions must be rejected by the caller before invoking this.
InkCacheProbe inspect_ink_cache(
    const MemoryRead& read,
    std::uint64_t resource_loader,
    std::string_view style_path,
    std::string_view property_name);
}
