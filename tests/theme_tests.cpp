#include "dreamui/theme.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <string>

struct MemoryInk final : dreamui::InkColorWriter {
    std::map<std::string, dreamui::Rgb> values = {
        {"MainColors.Red",{255,100,100}},
        {"MainColors.PanelBlue",{100,100,200}},
        {"MainColors.ActiveYellow",{230,200,50}}
    };
    std::string failWrite;
    bool read(std::string_view, std::string_view property, dreamui::Rgb& out) override {
        const auto it = values.find(std::string(property));
        if (it == values.end()) return false;
        out = it->second;
        return true;
    }
    bool write(std::string_view, std::string_view property, dreamui::Rgb color) override {
        auto it = values.find(std::string(property));
        if (it == values.end()) return false;
        it->second = color;
        if (property == failWrite) {
            // Simulates a native writer that reports failure after a partial write.
            failWrite.clear();
            return false;
        }
        return true;
    }
};

int main(int argc,char** argv) {
    if (argc!=2) return 2;
    const auto archive = dreamui::read_dream_package(argv[1]);
    if (!archive) { std::cerr << archive.error << '\n'; return 1; }
    auto parsed = dreamui::parse_theme(archive);
    if (!parsed) { std::cerr << parsed.error << '\n'; return 1; }
    if (parsed.theme.id != "dreamui.default" || parsed.theme.colors.size() < 20) return 1;
    if (parsed.theme.colors.at("text.primary") != dreamui::Rgb{0xEA,0xF0,0xF2}) return 1;
    {
        auto invalid = archive;
        invalid.entries.front().bytes = {'{','}'};
        if (dreamui::parse_theme(invalid)) return 1;
    }
    {
        auto invalid = archive;
        for (auto& entry: invalid.entries) {
            if (entry.name == "theme/colors.json") {
                entry.bytes = {'{','"','x','"',':','"','#','F','F','F','F','Z','Z','"','}'};
            }
        }
        if (dreamui::parse_theme(invalid)) return 1;
    }
    std::string err;
    if (dreamui::candidate_ink_bindings().size() != 3 ||
        dreamui::candidate_ink_bindings()[0].property != "MainColors.Red" ||
        dreamui::candidate_ink_bindings()[1].property != "MainColors.PanelBlue") return 1;
    MemoryInk fake;
    const auto vanilla = fake.values;
    dreamui::InkColorSession session;
    if (!session.apply(parsed.theme, fake, err)) { std::cerr << err << '\n'; return 1; }
    if (fake.values.at("MainColors.Red") != parsed.theme.colors.at("text.primary")) return 1;
    session.restore();
    if (fake.values != vanilla || session.active()) return 1;

    MemoryInk failing;
    failing.failWrite = "MainColors.PanelBlue";
    dreamui::InkColorSession failed;
    if (failed.apply(parsed.theme, failing, err)) return 1;
    if (failed.active() || failing.values != vanilla) return 1;

    std::cout << "Validated native theme and Ink mock apply/restore\n";
    return 0;
}
