#pragma once
#include "dreamui/ink_cache.hpp"
#include "dreamui/native_ink.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace dreamui {
// Read-only, bounded traversal of the native inkStyleResource dependency
// graph. Never takes ownership of a game handle or invokes a native function.
// A success is an observation, not authorization to edit the resource.
struct InkStyleGraphProbe {
    bool valid=false;
    bool found_color=false;
    std::uint32_t resources_visited=0;
    std::uint32_t references_seen=0;
    std::uint32_t references_loaded=0;
    std::uint32_t styles_visited=0;
    std::uint32_t properties_visited=0;
    std::uint64_t owner_path_hash=0;
    HdrColor color{};
    std::string reason;
};
InkStyleGraphProbe inspect_ink_style_graph(
    const MemoryRead& reader,
    std::uint64_t root_resource,
    std::uint64_t expected_root_path_hash,
    std::string_view target_property);
} // namespace dreamui
