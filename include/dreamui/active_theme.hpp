#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace dreamui {

// A small, independent preference file. Theme identities are always checked
// against the *validated* catalog before they can reach any game adapter.
// Empty/missing/invalid files mean the bundled default / catalog fallback.
bool valid_dream_id(std::string_view id) noexcept;
std::string read_selected_dream_id(const std::filesystem::path& path) noexcept;
bool write_selected_dream_id(const std::filesystem::path& path,
                             std::string_view id,std::string& error) noexcept;
}
