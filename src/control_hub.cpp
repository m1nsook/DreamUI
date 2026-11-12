#include "dreamui/control_hub.hpp"

namespace dreamui {
bool DreamCatalog::add(const Theme& theme, std::string& error) {
    if (theme.id.empty() || theme.name.empty()) {
        error="theme identity is missing";
        return false;
    }
    const auto inserted = entries_.emplace(theme.id,DreamSummary{theme.id,theme.name,theme.version}).second;
    if (!inserted) error="duplicate theme id: " + theme.id;
    return inserted;
}
ControlHubModel DreamCatalog::view(std::string_view requested_active) const {
    ControlHubModel model;
    model.version=std::string(kDreamUIVersion);
    for (const auto& [id,summary]:entries_) model.installed.push_back(summary);
    if (entries_.empty()) return model;
    model.has_themes=true;
    auto it=entries_.find(std::string(requested_active));
    if (it==entries_.end()) it=entries_.find("dreamui.default");
    if (it==entries_.end()) it=entries_.begin();
    model.active_id=it->second.id;
    model.active_name=it->second.name;
    return model;
}
}
