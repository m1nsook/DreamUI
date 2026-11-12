#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dreamui {
struct PackageEntry {
    std::string name;
    std::vector<std::uint8_t> bytes;
};
struct PackageResult {
    std::vector<PackageEntry> entries;
    std::string error;
    explicit operator bool() const { return error.empty(); }
    const PackageEntry* find(std::string_view name) const;
};
// v0.1 deliberately accepts only non-encrypted ZIP_STORED members.
// This prevents surprises from arbitrary decompression code in the game process.
PackageResult read_dream_package(const std::filesystem::path& path);
}
