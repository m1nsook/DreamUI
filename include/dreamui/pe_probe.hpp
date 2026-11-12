#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace dreamui {
// Read-only analysis of the on-disk Windows executable. Not an Ink RTTI/engine
// object resolver. Never use string hits as function pointers or patch sites.
struct PeProbe {
    bool valid = false;
    bool x64 = false;
    std::uint32_t coff_timestamp = 0;
    std::uint32_t image_size = 0;
    std::size_t rdata_bytes_scanned = 0;
    bool ink_style_resource_literal = false;
    bool ink_style_property_literal = false;
    bool ink_widget_literal = false;
    std::string error;
};
PeProbe inspect_pe(std::span<const std::uint8_t> image);
} // namespace dreamui
