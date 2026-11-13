#pragma once
#include "dreamui/theme.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dreamui {

// Matches the 16-byte, 16-byte-aligned native REDengine HDRColor payload.
// Preserve all original channels exactly, including HDR values > 1 and alpha.
struct alignas(16) HdrColor {
    float red = 0;
    float green = 0;
    float blue = 0;
    float alpha = 1;
    bool operator==(const HdrColor&) const = default;
};
static_assert(sizeof(HdrColor) == 0x10);
static_assert(alignof(HdrColor) == 0x10);

bool finite_hdr(const HdrColor& value) noexcept;
HdrColor preview_sdr_color(Rgb color, float original_alpha) noexcept;

// An identity must be provided by a VERIFIED engine resource lifecycle event.
// Do not treat a cached memory address alone as a safe, owning handle.
struct ResourceIdentity {
    std::uint64_t id = 0;
    std::uint64_t generation = 0;
    bool operator==(const ResourceIdentity&) const = default;
    explicit operator bool() const noexcept { return id != 0 && generation != 0; }
};

enum class InkResourceEventKind { Loaded, Ready, Unloaded };
struct InkResourceEvent {
    ResourceIdentity resource;
    std::string_view path;
    InkResourceEventKind kind;
    // True ONLY if the engine-side bridge authenticated inkStyleResource.
    bool verified_ink_style_resource = false;
};

// Future, game-thread-only native bridge contract. No game API calls are
// implemented here; the current ASI must never construct a fake backend.
// read_hdr_color MUST validate both inkStyleProperty and Variant HDRColor RTTI.
// write_hdr_color MUST use the game allocator/type-aware Variant machinery.
// refresh_resource MUST honor the engine's scheduling and lifetime rules.
class InkEngineAccess {
public:
    virtual ~InkEngineAccess() = default;
    virtual bool on_engine_thread() const noexcept = 0;
    virtual bool read_hdr_color(ResourceIdentity resource,
                                std::string_view property, HdrColor& out) noexcept = 0;
    virtual bool write_hdr_color(ResourceIdentity resource,
                                 std::string_view property, HdrColor color) noexcept = 0;
    virtual bool refresh_resource(ResourceIdentity resource) noexcept = 0;
};

// One visible color, reversible and scoped to ONE resource generation.
// Events never cause automatic game writes; apply is opt-in. A failed
// restoration retains the original snapshot for a later retry.
class NativeInkColorSession {
public:
    static constexpr std::string_view kStylePath =
        R"(base\gameplay\gui\common\main_colors.inkstyle)";
    static constexpr std::string_view kFirstProperty = "MainColors.Red";
    static constexpr std::string_view kThemeToken = "text.primary";

    void on_resource_event(const InkResourceEvent& event) noexcept;
    bool ready() const noexcept { return ready_; }
    bool active() const noexcept { return saved_.has_value(); }
    bool apply_first_color(const Theme& theme, InkEngineAccess& engine, std::string& error);
    bool restore(InkEngineAccess& engine, std::string& error);

private:
    struct Snapshot {
        ResourceIdentity resource;
        HdrColor original;
    };
    ResourceIdentity current_{};
    bool ready_ = false;
    std::optional<Snapshot> saved_;
};
} // namespace dreamui
