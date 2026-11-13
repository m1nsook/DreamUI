#pragma once
#include "dreamui/ink_cache.hpp"
#include "dreamui/native_ink.hpp"
#include <cstdint>
#include <functional>
#include <string_view>

namespace dreamui {

// Callback supplied only by the game's *own* serializer lifecycle handler.
// The caller must first verify a strong, still-live CResource handle and the
// object's native RTTI type == inkStyleResource. Never pass raw cache pointers.
using MemoryWrite = std::function<bool(std::uint64_t, const void*, std::size_t)>;

enum class InkLoadPatchStatus {
    NoStyles,
    NoMatchingProperty,
    Applied,
    AlreadyMatches,
    InvalidResource,
    InvalidProperty,
    WriteFailed,
    RollbackFailed
};

struct InkLoadPatchResult {
    InkLoadPatchStatus status = InkLoadPatchStatus::InvalidResource;
    std::uint32_t matched = 0;
    std::uint32_t written = 0;
    HdrColor original{};
    HdrColor preview{};
};

// Strict bounded, transactionally reversible *load-stage* mutation.
// This does NOT replace a Variant or trigger a refresh: it updates only the
// existing, verified native HDRColor allocation before resource finalization.
// Only invoke during an authenticated engine serialization callback; never
// from the Tab/file-command worker.
InkLoadPatchResult patch_loaded_ink_color(
    const MemoryRead& reader,
    const MemoryWrite& writer,
    std::uint64_t style_resource,
    std::uint64_t resource_path_hash_expected,
    std::string_view property,
    Rgb chosen_color);
}
