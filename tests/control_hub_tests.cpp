#include "dreamui/control_hub.hpp"
#include <string>
int main(){
    dreamui::DreamCatalog catalog;
    auto empty=catalog.view("dreamui.default");
    if (empty.has_themes || !empty.installed.empty()) return 1;
    std::string error;
    dreamui::Theme first; first.id="x.frostline"; first.name="Frostline"; first.version="1.0.0";
    dreamui::Theme second;second.id="dreamui.default";second.name="DreamUI Default";second.version="0.1.0";
    if(!catalog.add(first,error) || !catalog.add(second,error))return 1;
    if(catalog.add(first,error) || error.find("duplicate")==std::string::npos)return 1;
    auto fallback=catalog.view("missing");
    if(!fallback.has_themes || fallback.installed.size()!=2 ||fallback.active_id!="dreamui.default")return 1;
    auto selected=catalog.view("x.frostline");
    if(selected.active_name!="Frostline" || selected.title!="DreamUI" || selected.version!="v0.1.0-alpha.6")return 1;
}
