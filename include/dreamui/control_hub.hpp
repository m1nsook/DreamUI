#pragma once
#include "dreamui/theme.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace dreamui {
inline constexpr std::string_view kDreamUIVersion = "v0.1.0-alpha.5";
struct DreamSummary {
    std::string id;
    std::string name;
    std::string version;
};
struct ControlHubModel {
    std::string title = "DreamUI";
    std::string version;
    std::vector<DreamSummary> installed;
    std::string active_id;
    std::string active_name;
    bool has_themes = false;
};
// Pure data layer for a future native-Ink menu tile. No UI widgets/overlays.
class DreamCatalog {
public:
    bool add(const Theme& theme, std::string& error);
    ControlHubModel view(std::string_view requested_active) const;
private:
    std::map<std::string,DreamSummary> entries_;
};
}
