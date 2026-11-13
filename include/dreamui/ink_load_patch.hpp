#pragma once
#include "dreamui/ink_cache.hpp"
#include "dreamui/native_ink.hpp"
#include <cstdint>
#include <array>
#include <functional>
#include <string_view>
#include <span>

namespace dreamui {

// Callback supplied only by the game's *own* serializer lifecycle handler.
// The caller must first verify a strong, still-live CResource handle and the
// object's native RTTI type == inkStyleResource. Never pass raw cache pointers.
using MemoryWrite = std::function<bool(std::uint64_t, const void*, std::size_t)>;

// A journal is valid ONLY while the same live, game-owned serializer
// resource is authenticated again. Stored raw addresses are NOT owning
// references and must never be used on the diagnostics worker thread.
struct InkLoadJournalEntry {
    std::uint64_t color_address = 0;
    std::uint64_t type_address = 0;
    std::uint64_t property_path_hash = 0;
    std::uint32_t style_index = 0;
    std::uint32_t property_index = 0;
    HdrColor before{};
    HdrColor applied{};
};
struct InkLoadJournal {
    std::uint64_t resource_address = 0;
    std::uint64_t resource_path_hash = 0;
    std::uint64_t resource_instance_id = 0;
    std::uint32_t count = 0;
    std::array<InkLoadJournalEntry,32> entries{};
    bool active() const noexcept { return count != 0; }
    void clear() noexcept { *this = {}; }
};

enum class InkLoadRestoreStatus {
    NoJournal, InvalidResource, StaleResource, ChangedExternally,
    Restored, WriteFailed, RollbackFailed
};

enum class InkLoadPatchStatus {
    NoStyles,
    NoMatchingProperty,
    Applied,
    AlreadyMatches,
    InvalidResource,
    InvalidProperty,
    WriteFailed,
    RollbackFailed,
    JournalOccupied
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

struct InkPaletteEntry {
    std::string_view native_property;
    Rgb theme_color;
};
// Atomically patch up to 16 distinct named native HDRColor properties.
// All requested names must exist and pass verification before ANY write;
// this avoids mixed palettes if a target is absent from a game build.
InkLoadPatchResult patch_loaded_ink_palette(
    const MemoryRead& reader,
    const MemoryWrite& writer,
    std::uint64_t style_resource,
    std::uint64_t resource_path_hash_expected,
    std::span<const InkPaletteEntry> targets,
    InkLoadJournal* journal = nullptr);

InkLoadPatchResult patch_loaded_ink_color(
    const MemoryRead& reader,
    const MemoryWrite& writer,
    std::uint64_t style_resource,
    std::uint64_t resource_path_hash_expected,
    std::string_view property,
    Rgb chosen_color,
    InkLoadJournal* journal = nullptr);

// Requires the same exact live resource, game callback, and serialized
// instance ID. Validates ALL property locations and previews before write;
// errors never discard the journal and failed restores attempt rollback.
InkLoadRestoreStatus restore_loaded_ink_color(
    const MemoryRead& reader,
    const MemoryWrite& writer,
    std::uint64_t authenticated_resource,
    const InkLoadJournal& journal);

// Caller clears the journal ONLY after receiving Restored or after the
// entire owning resource generation has been unloaded by the engine.
}
