#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/zip_reader.hpp"
#include "dreamui/theme.hpp"
#include "dreamui/control_hub.hpp"
#include "dreamui/win_probe.hpp"
#include "dreamui/dev/command_registry.hpp"
#include "dreamui/dev/command_inbox.hpp"
#include <atomic>
#include <exception>
#include <filesystem>
#include <cstdint>
#include <span>
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
        log << "Diagnostics: plain TAB or developer command file; "
               "no automatic scans, no DX12 hooks, no game patches.\n";
        log.flush();

        // Fresh diagnostic log for every launch, not an enormous append-only
        // history of previous alpha test runs.
        std::ofstream probe(home / L"logs" / L"DreamUI-probe.log", std::ios::trunc);
        if (!probe) {
            log << "Unable to open DreamUI-probe.log; hotkey unavailable\n";
            return 0;
        }
        // The developer-contributed command registry is independent of
        // ImGui, MinHook and DX12. The command inbox is only an optional
        // local text file; no game-input hook or rendering changes exist.
        fs::create_directories(home / L"dev");
        std::ofstream console(home / L"logs" / L"DreamUI-dev.log", std::ios::trunc);
        dreamui::dev::CommandInbox inbox;
        const auto inbox_path = home / L"dev" / L"commands.txt";
        inbox.poll(inbox_path); // never run an old command from prior sessions
        dreamui::dev::CommandRegistry registry;
        std::string phase = "unspecified";
        bool requested = false;
        std::uint64_t requests = 0;

        dreamui::dev::CommandContext context;
        context.request_refresh = &requested;
        context.append = [&](std::string line) {
            if (console) {
                console << line << '\n';
                console.flush();
            }
        };
        registry.add("help", "list available DreamUI commands",
            [&](auto& ctx,auto){ctx.append(registry.help_text());});
        registry.add("version", "show DreamUI native ASI version",
            [&](auto& ctx,auto) {
                ctx.append("DreamUI " + std::string(dreamui::kDreamUIVersion));
            });
        registry.add("themes", "show validated .dream themes",
            [&](auto& ctx,auto) {
                if (hub.installed.empty()) {
                    ctx.append("no validated .dream themes");
                } else {
                    for (const auto& item : hub.installed)
                        ctx.append(item.name + " (" + item.id + ") v" + item.version);
                }
            });
        registry.add("status", "show session state; this is not a game-screen detector",
            [&](auto& ctx,auto) {
                ctx.append("diagnostic requests=" + std::to_string(requests) +
                           " | last phase label=" + phase +
                           " | verified themes=" + std::to_string(accepted) +
                           " | engine changes=disabled");
            });
        registry.add("probe", "probe [phase-label] - read-only Ink graph snapshot",
            [&](auto& ctx,std::span<const std::string> args) {
                if (args.size()>1 || (!args.empty() && args.front().size()>48)) {
                    ctx.append("usage: probe [single-phase-label]");
                    return;
                }
                phase = args.empty() ? "unspecified" : args.front();
                if (ctx.request_refresh) *ctx.request_refresh = true;
            });
        registry.add("clear", "clear this session's developer command log",
            [&](auto& ctx,auto) {
                if (console) {
                    console.close();
                    console.open(home / L"logs" / L"DreamUI-dev.log",std::ios::trunc);
                }
                ctx.append("developer command log cleared");
            });

        auto run_probe = [&](std::string_view source) {
            ++requests;
            probe << "=== DreamUI " << dreamui::kDreamUIVersion
                  << " | " << source << " probe #" << requests
                  << " | phase label=" << phase << " (user supplied, not engine-verified)"
                  << " ===\n";
            dreamui::log_executable_probe(fs::path(exe), probe);
            probe << "=== probe #" << requests << " complete ===\n";
            probe.flush();
            context.append("probe #" + std::to_string(requests) +
                           " complete; phase label=" + phase +
                           "; results in DreamUI-probe.log");
        };
        auto dispatch = [&](std::string_view command) {
            requested = false;
            std::string outcome;
            const auto ok = registry.dispatch(context,command,outcome);
            context.append("> " + std::string(command) + " => " +
                           (ok ? "" : "! ") + outcome);
            if (requested)run_probe("COMMAND");
        };

        context.append("DreamUI developer commands ready. Create or edit "
                       "dreamui/dev/commands.txt after game launch; "
                       "commands: help, version, themes, status, probe [phase], clear");
        bool was_down = modifier_down(VK_TAB);
        while (running.load(std::memory_order_relaxed)) {
            const auto batch = inbox.poll(inbox_path);
            if (batch.ready) {
                if (!batch.error.empty())context.append("inbox error: "+batch.error);
                for (const auto& cmd : batch.lines)dispatch(cmd);
            }
            bool down = modifier_down(VK_TAB);
            // Tab is NOT swallowed; neither Alt+Tab nor Shift+Tab runs probes.
            if (down && !was_down && is_game_foreground() &&
                !modifier_down(VK_MENU) && !modifier_down(VK_SHIFT) &&
                !modifier_down(VK_LWIN) && !modifier_down(VK_RWIN)) {
                phase = "hotkey";
                run_probe("TAB");
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
