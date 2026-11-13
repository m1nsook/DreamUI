#pragma once
#include "dreamui/ink_cache.hpp"
#include "dreamui/native_ink.hpp"
#include <cstdint>
#include <vector>

namespace dreamui {

// Copy of the *entire* style/property tree ONLY when every Variant has a
// supported native value representation. No stored pointers, handles,
// allocator headers, RTTI pointers, or borrowed native memory.
// This snapshot is NOT an engine-side DynArray and cannot be assigned to the
// resource or used to call PostLoad.
struct InkCapturedProperty {
    std::uint64_t property_path_hash=0;
    HdrColor color{};
};
struct InkCapturedStyle {
    std::uint64_t style_id=0;
    std::uint64_t state=0;
    std::uint32_t first_property=0;
    std::uint32_t property_count=0;
};
struct InkOwnedStyleTree {
    std::uint64_t resource_path_hash=0;
    std::uint64_t resource_instance_id=0;
    std::vector<InkCapturedStyle> styles;
    std::vector<InkCapturedProperty> properties;
    bool complete() const noexcept {
        return resource_path_hash!=0 && resource_instance_id!=0 &&
               !styles.empty();
    }
    void clear() noexcept {
        resource_path_hash=0;
        resource_instance_id=0;
        styles.clear();
        properties.clear();
    }
};

enum class InkStyleCaptureStatus {
    Captured, InvalidResource, NoStyles, InvalidVariant,
    UnsupportedVariant, LimitExceeded
};
struct InkStyleCaptureResult {
    InkStyleCaptureStatus status=InkStyleCaptureStatus::InvalidResource;
    std::uint32_t observed_styles=0;
    std::uint32_t observed_properties=0;
    std::uint32_t unsupported_properties=0;
};

// Read-only, bounded, all-or-nothing copier. The caller must verify the
// CResource is an owning, live inkStyleResource at a game-owned lifecycle
// callback; this function does not authenticate pointers on its own.
// The first unsupported/invalid Variant rejects the entire tree. Currently
// the only supported populated Variant is externally allocated HDRColor.
// Empty style property arrays are allowed; empty/other Variants are not.
// No engine methods, allocation callbacks, or memory writes are performed.
InkStyleCaptureResult capture_loaded_ink_style_tree(
    const MemoryRead& read,
    std::uint64_t authenticated_resource,
    std::uint64_t expected_resource_path_hash,
    InkOwnedStyleTree& destination);

} // namespace dreamui
