#pragma once
#include "dreamui/theme.hpp"
#include <filesystem>
#include <ostream>
#include <string>

namespace dreamui {

// Windows-only independent engine serializer bridge. Disabled in ordinary
// DreamUI builds; compiled in special CI configurations for ABI inspection.
// An explicit per-install developer marker is required even when compiled in.
// Never call from DllMain: it maps the executable and installs a code hook.
bool start_native_resource_stage(
    const std::filesystem::path& game_exe,
    const std::filesystem::path& dreamui_home,
    const Theme& theme,
    std::ostream& log) noexcept;

// These commands change intent only. Game memory is touched solely by
// a later authenticated serializer callback. They do NOT immediately refresh
// already-instantiated Ink widgets.
bool native_resource_stage_request_apply() noexcept;
bool native_resource_stage_request_restore() noexcept;
std::string native_resource_stage_status();
}
