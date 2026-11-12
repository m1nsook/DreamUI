#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/zip_reader.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace {
DWORD WINAPI initialize(LPVOID context) {
    const auto module = static_cast<HMODULE>(context);
    wchar_t location[32768]{};
    if (!GetModuleFileNameW(module,location,32768)) return 0;
    try {
        // DreamUI.asi lives in <game>/bin/x64/plugins/.
        const fs::path pluginPath(location);
        const fs::path gameRoot=pluginPath.parent_path().parent_path().parent_path().parent_path();
        const fs::path home=gameRoot / L"dreamui";
        fs::create_directories(home / L"logs");
        std::ofstream log(home / L"logs" / L"DreamUI.log",std::ios::app);
        if (!log) return 0;
        log << "DreamUI native loader prototype 0.1 | no game patches enabled\n";
        wchar_t exe[32768]{};
        GetModuleFileNameW(nullptr,exe,32768);
        if (fs::path(exe).filename()!=L"Cyberpunk2077.exe") {
            log << "No Cyberpunk2077.exe host; refusing game initialization\n";
            return 0;
        }
        const auto themeDir=home / L"themes";
        if (!fs::exists(themeDir)) { log << "No themes directory yet\n"; return 0; }
        for (const auto& entry:fs::directory_iterator(themeDir)) {
            if (!entry.is_regular_file() || entry.path().extension()!=L".dream") continue;
            auto package=dreamui::read_dream_package(entry.path());
            if (!package) {
                log << "Rejected " << entry.path().filename().string() << ": " << package.error << "\n";
                continue;
            }
            log << "Accepted archive " << entry.path().filename().string() << " (" << package.entries.size() << " members)\n";
        }
        log << "Native startup probe completed; Ink hooks not yet implemented\n";
    } catch (const std::exception&) {
        // Never crash the host game due to a theme-loader or filesystem exception.
    }
    return 0;
}
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,initialize,module,0,nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
