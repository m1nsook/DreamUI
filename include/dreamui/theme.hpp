#pragma once
#include "dreamui/zip_reader.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace dreamui {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb&) const = default;
};
struct Theme {
    std::string id, name, version;
    std::map<std::string, Rgb> colors;
};
struct ThemeResult {
    Theme theme;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};

// Parses/validates manifest.json and theme/colors.json from a .dream archive.
// Invalid archives must never reach native game-facing code.
ThemeResult parse_theme(const PackageResult& archive);

struct InkBinding {
    std::string_view token;
    std::string_view resource;
    std::string_view property;
};
// These are candidate bindings to documented vanilla Ink style properties;
// resource availability and write safety must be verified in-game.
const std::vector<InkBinding>& candidate_ink_bindings();

// Game-specific integration is intentionally an injected interface.
// No unverified game offsets, third-party loader APIs or implicit memory writes.
class InkColorWriter {
public:
    virtual ~InkColorWriter() = default;
    virtual bool read(std::string_view resource, std::string_view property, Rgb& result) = 0;
    virtual bool write(std::string_view resource, std::string_view property, Rgb value) = 0;
};

class InkColorSession {
public:
    bool apply(const Theme& theme, InkColorWriter& writer, std::string& error);
    void restore() noexcept;
    bool active() const { return !originals_.empty(); }
private:
    struct Saved {
        InkBinding binding;
        Rgb color;
    };
    InkColorWriter* writer_ = nullptr;
    std::vector<Saved> originals_;
};

} // namespace dreamui
