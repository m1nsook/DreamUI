#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/zip_reader.hpp"
#include "dreamui/theme.hpp"
#include "dreamui/control_hub.hpp"
#include "dreamui/win_probe.hpp"
#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace {
std::atomic_bool running{true};
bool is_game_foreground() {
    const HWND foreground = GetForegroundWindow();
    DWORD pid = 0;
    if (!foreground) return false;
    GetWindowThreadProcessId(foreground, &pid);
    return pid == GetCurrentProcessId();
}
bool modifier_down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
DWORD WINAPI initialize(LPVOID context) {
    const auto module = static_cast<HMODULE>(context);
    wchar_t location[32768]{};
    if (!GetModuleFileNameW(module, location, 32768)) return 0;
    try {
        const fs::path plugin_path(location);
        const fs::path game_root =
            plugin_path.parent_path().parent_path().parent_path().parent_path();
        const fs::path home = game_root / L"dreamui";
        fs::create_directories(home / L"logs");
        std::ofstream log(home / L"logs" / L"DreamUI.log", std::ios::app);
        if (!log) return 0;
        wchar_t exe[32768]{};
        if (!GetModuleFileNameW(nullptr, exe, 32768) ||
            fs::path(exe).filename() != L"Cyberpunk2077.exe") {
            log << "DreamUI: unexpected game host; no diagnostic input installed\n";
            return 0;
        }
        log << "DreamUI " << dreamui::kDreamUIVersion
            << " | on-demand Tab diagnostics enabled; no game patches\n";
        dreamui::DreamCatalog catalog;
        const auto theme_dir = home / L"themes";
        unsigned accepted = 0;
        if (fs::exists(theme_dir)) {
            for (const auto& entry : fs::directory_iterator(theme_dir)) {
                if (!entry.is_regular_file() || entry.path().extension() != L".dream") continue;
                const auto package = dreamui::read_dream_package(entry.path());
                if (!package) {
                    log << "Rejected archive " << entry.path().filename().string()
                        << ": " << package.error << "\n";
                    continue;
                }
                auto parsed = dreamui::parse_theme(package);
                if (!parsed) {
                    log << "Rejected theme " << entry.path().filename().string()
                        << ": " << parsed.error << "\n";
                    continue;
                }
                std::string error;
                if (!catalog.add(parsed.theme, error)) {
                    log << "Rejected theme: " << error << "\n";
                    continue;
                }
                ++accepted;
                log << "Validated theme " << parsed.theme.id
                    << " | " << parsed.theme.colors.size() << " color tokens\n";
            }
        }
        const auto hub = catalog.view("dreamui.default");
        log << "Themes available=" << accepted
            << " active=" << (hub.has_themes ? hub.active_name : "(none)")
            << "\n";
        log << "Diagnostics: press plain TAB with Cyberpunk focused. "
               "No timed scans, no input interception. Results: DreamUI-probe.log\n";
        log.flush();

        // Fresh diagnostic log for every launch, not an enormous append-only
        // history of previous alpha test runs.
        std::ofstream probe(home / L"logs" / L"DreamUI-probe.log", std::ios::trunc);
        if (!probe) {
            log << "Unable to open DreamUI-probe.log; hotkey unavailable\n";
            return 0;
        }
        bool was_down = modifier_down(VK_TAB);
        unsigned requests = 0;
        while (running.load(std::memory_order_relaxed)) {
            bool down = modifier_down(VK_TAB);
            // Do not treat Alt+Tab or Shift+Tab (Steam overlay) as a request.
            // Nothing is hooked or consumed: Cyberpunk still gets normal Tab.
            if (down && !was_down && is_game_foreground() &&
                !modifier_down(VK_MENU) && !modifier_down(VK_SHIFT) &&
                !modifier_down(VK_LWIN) && !modifier_down(VK_RWIN)) {
                ++requests;
                probe << "=== DreamUI " << dreamui::kDreamUIVersion
                      << " | TAB probe #" << requests << " ===\n";
                dreamui::log_executable_probe(fs::path(exe), probe);
                probe << "=== probe #" << requests << " complete ===\n";
                probe.flush();
                down = modifier_down(VK_TAB);
            }
            was_down = down;
            Sleep(30);
        }
        log << "DreamUI diagnostic input stopped\n";
    } catch (const std::exception&) {
        // Avoid unwinding exceptions through Cyberpunk's process entry points.
    }
    return 0;
}
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        running.store(true, std::memory_order_relaxed);
        HANDLE thread = CreateThread(nullptr, 0, initialize, module, 0, nullptr);
        if (thread) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        running.store(false, std::memory_order_relaxed);
    }
    return TRUE;
}
