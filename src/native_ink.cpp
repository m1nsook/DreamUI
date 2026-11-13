#include "dreamui/native_ink.hpp"
#include <cmath>

namespace dreamui {

bool finite_hdr(const HdrColor& color) noexcept {
    // HDR channels can be larger than 1.0. A finite float bound protects
    // backend conversions from pathological values without losing HDR colors.
    constexpr float kMax = 65504.0f;
    return std::isfinite(color.red) && std::isfinite(color.green) &&
           std::isfinite(color.blue) && std::isfinite(color.alpha) &&
           std::abs(color.red) <= kMax && std::abs(color.green) <= kMax &&
           std::abs(color.blue) <= kMax && std::abs(color.alpha) <= kMax;
}

HdrColor preview_sdr_color(Rgb color, float original_alpha) noexcept {
    // This is a tentative SDR-to-float conversion ONLY. Engine color-space
    // calibration is required before using it for actual Ink mutations.
    constexpr float kScale = 1.0f / 255.0f;
    return {color.r * kScale, color.g * kScale, color.b * kScale, original_alpha};
}

void NativeInkColorSession::on_resource_event(const InkResourceEvent& event) noexcept {
    if (event.path != kStylePath || !event.resource)
        return;
    if (event.kind == InkResourceEventKind::Unloaded) {
        if (current_ == event.resource) {
            // The engine has invalidated this generation. NEVER write to it.
            saved_.reset();
            current_ = {};
            ready_ = false;
        }
        return;
    }
    if (!event.verified_ink_style_resource)
        return;
    if (saved_ && event.resource != current_) {
        // Do not silently abandon a dirty prior resource on a new event.
        return;
    }
    if (event.kind == InkResourceEventKind::Loaded) {
        current_ = event.resource;
        ready_ = false;
        // A second load of the same generation invalidates previously
        // observed readiness; no attempt is made to use stale data.
        return;
    }
    if (event.kind == InkResourceEventKind::Ready) {
        current_ = event.resource;
        ready_ = true;
    }
}

bool NativeInkColorSession::apply_first_color(
    const Theme& theme, InkEngineAccess& engine, std::string& error) {
    error.clear();
    if (!engine.on_engine_thread()) {
        error = "refusing Ink operation outside the engine thread"; return false;
    }
    if (!ready_ || !current_) {
        error = "native inkStyleResource has not received a verified Ready event"; return false;
    }
    if (saved_) {
        error = "a color change is still active or needs restoration"; return false;
    }
    const auto it = theme.colors.find(std::string(kThemeToken));
    if (it == theme.colors.end()) {
        error = "theme missing text.primary"; return false;
    }
    HdrColor original{};
    if (!engine.read_hdr_color(current_, kFirstProperty, original)) {
        error = "HDRColor Variant read or RTTI validation failed"; return false;
    }
    if (!finite_hdr(original)) {
        error = "native HDRColor contains invalid float channels"; return false;
    }

    const auto desired = preview_sdr_color(it->second, original.alpha);
    // Journal the FULL typed original before writing, because a native
    // writer may mutate the value before returning an error.
    saved_ = Snapshot{current_, original};
    if (!engine.write_hdr_color(current_, kFirstProperty, desired)) {
        error = "native HDRColor write failed";
        std::string rollback_error;
        if (!restore(engine, rollback_error))
            error += "; automatic rollback pending: " + rollback_error;
        return false;
    }
    if (!engine.refresh_resource(current_)) {
        error = "native Ink resource refresh failed";
        std::string rollback_error;
        if (!restore(engine, rollback_error))
            error += "; automatic rollback pending: " + rollback_error;
        return false;
    }
    return true;
}

bool NativeInkColorSession::restore(
    InkEngineAccess& engine, std::string& error) {
    error.clear();
    if (!saved_)
        return true;
    if (!engine.on_engine_thread()) {
        error = "refusing Ink restoration outside the engine thread"; return false;
    }
    if (!ready_ || !current_ || !(current_ == saved_->resource)) {
        error = "original Ink resource generation is no longer ready"; return false;
    }
    if (!engine.write_hdr_color(saved_->resource, kFirstProperty, saved_->original)) {
        error = "native original HDRColor restore failed; retry remains available";
        return false;
    }
    if (!engine.refresh_resource(saved_->resource)) {
        error = "native Ink refresh after restoration failed; retry remains available";
        return false;
    }
    saved_.reset();
    return true;
}

} // namespace dreamui
