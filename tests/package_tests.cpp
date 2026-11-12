#include "dreamui/zip_reader.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if (argc!=2) return 2;
    const auto root=std::filesystem::path(argv[1]);
    // Native reader must never accept bogus archives.
    auto missing=dreamui::read_dream_package(root / "missing.dream");
    if (missing || missing.error.empty()) return 1;
    // Python test creates test package before CTest in CI.
    auto valid=dreamui::read_dream_package(root / "../../dist/Default.dream");
    if (!valid || !valid.find("manifest.json") || !valid.find("theme/colors.json")) {
        std::cerr << "Could not read valid package: " << valid.error << '\n'; return 1;
    }
    return 0;
}
