#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include "dreamui/win_probe.hpp"
#include "dreamui/pe_probe.hpp"
#include "dreamui/address_db.hpp"
#include "dreamui/ink_cache.hpp"
#include "dreamui/ink_graph.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <vector>
#include <array>

namespace dreamui {
namespace {
struct FileHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~FileHandle() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
struct MappingHandle {
    HANDLE handle = nullptr;
    ~MappingHandle() { if (handle) CloseHandle(handle); }
};
struct View {
    const void* address = nullptr;
    ~View() { if (address) UnmapViewOfFile(address); }
};
struct Resolver {
    bool enabled = false;
    std::uint32_t loader_rva = 0;
    std::string error;
};
Resolver load_resolver(const std::filesystem::path& executable, std::ostream& log) {
    Resolver out;
    log << "Resolving executable fingerprint and native ResourceLoader address (once per session)\n";
    DWORD ignored = 0;
    const auto resource_size = GetFileVersionInfoSizeW(executable.c_str(), &ignored);
    if (resource_size && resource_size <= 1024 * 1024) {
        std::vector<std::uint8_t> info(resource_size);
        if (GetFileVersionInfoW(executable.c_str(), 0, resource_size, info.data())) {
            void* value = nullptr;
            UINT length = 0;
            if (VerQueryValueW(info.data(), L"\\", &value, &length) &&
                value && length >= sizeof(VS_FIXEDFILEINFO)) {
                const auto* fixed = static_cast<const VS_FIXEDFILEINFO*>(value);
                if (fixed->dwSignature == 0xFEEF04BD) {
                    log << "Executable file version: "
                        << HIWORD(fixed->dwFileVersionMS) << '.'
                        << LOWORD(fixed->dwFileVersionMS) << '.'
                        << HIWORD(fixed->dwFileVersionLS) << '.'
                        << LOWORD(fixed->dwFileVersionLS) << "\n";
                }
            }
        }
    }
    FileHandle file{CreateFileW(executable.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file.handle == INVALID_HANDLE_VALUE) {
        out.error = "game executable could not be opened"; return out;
    }
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file.handle, &length) || length.QuadPart < 64 ||
        length.QuadPart > 1024LL * 1024 * 1024) {
        out.error = "game executable size invalid"; return out;
    }
    MappingHandle mapping{CreateFileMappingW(file.handle, nullptr, PAGE_READONLY, 0, 0, nullptr)};
    if (!mapping.handle) { out.error = "read-only file mapping failed"; return out; }
    View view{MapViewOfFile(mapping.handle, FILE_MAP_READ, 0, 0, 0)};
    if (!view.address) { out.error = "read-only file view failed"; return out; }
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(view.address),
        static_cast<std::size_t>(length.QuadPart));
    const auto pe = inspect_pe(bytes);
    if (!pe.valid) { out.error = "invalid PE: " + pe.error; return out; }
    log << "Engine PE: timestamp=" << pe.coff_timestamp
        << " image_size=" << pe.image_size << "\n";
    // Only the executable fingerprint confirmed by the project owner's
    // alpha.3 through alpha.7 logs has a verified read-only layout.
    if (pe.coff_timestamp != 1792368047u || pe.image_size != 92385280u) {
        out.error = "unsupported executable fingerprint; memory probe disabled";
        return out;
    }
    constexpr std::uint32_t kResourceLoader = 0x783C1034;
    constexpr std::array<std::uint32_t, 1> wanted{kResourceLoader};
    const auto db = read_address_database(
        executable.parent_path() / L"cyberpunk2077_addresses.json",
        pe.coff_timestamp, pe.sections, wanted);
    if (!db.valid) {
        out.error = "game address database rejected: " + db.error;
        return out;
    }
    const auto it = db.matches.find(kResourceLoader);
    if (it == db.matches.end() || it->second.segment != 3 ||
        it->second.rva >= pe.image_size ||
        pe.image_size - it->second.rva < sizeof(void*)) {
        out.error = "ResourceLoader global not in validated data section";
        return out;
    }
    out.loader_rva = it->second.rva;
    out.enabled = true;
    log << "Address DB: linker timestamp matched; ResourceLoader global RVA="
        << out.loader_rva << " (no game function calls)\n";
    return out;
}
bool safe_read(std::uint64_t address, void* dest, std::size_t length) {
    if (address < 0x10000 || length == 0 || length > 4096 ||
        address > UINT64_MAX - length) return false;
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
        dest, length, &copied) && copied == length;
}
bool mapped_pointer(std::uintptr_t pointer) {
    if (pointer < 0x10000) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(pointer), &info, sizeof(info)) != sizeof(info))
        return false;
    const auto protection = info.Protect & 0xff;
    return info.State == MEM_COMMIT && !(info.Protect & PAGE_GUARD) &&
        protection != PAGE_NOACCESS;
}
}

void log_executable_probe(const std::filesystem::path& executable, std::ostream& log) noexcept {
    // Invoked ONLY on an actual Tab keypress, while Cyberpunk is foreground.
    // The resolver is cached, so subsequent presses don't rescan the giant
    // address database or the executable.
    static std::optional<Resolver> resolver;
    try {
        if (!resolver) {
            resolver = load_resolver(executable, log);
        }
        if (!resolver->enabled) {
            log << "Ink probe unavailable: " << resolver->error << "\n";
            return;
        }
        const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!module || module > UINT64_MAX - resolver->loader_rva) {
            log << "Ink probe unavailable: executable image base invalid\n";
            return;
        }
        std::uintptr_t singleton = 0;
        if (!safe_read(module + resolver->loader_rva, &singleton, sizeof(singleton))) {
            log << "ResourceLoader: global pointer read failed\n";
            return;
        }
        if (!singleton) {
            log << "ResourceLoader: null; press Tab again after entering the main menu\n";
            return;
        }
        if (!mapped_pointer(singleton)) {
            log << "ResourceLoader: pointer not mapped/readable; probe skipped\n";
            return;
        }
        const MemoryRead reader = [](std::uint64_t address, void* destination, std::size_t size) {
            return safe_read(address, destination, size);
        };
        const auto style = inspect_ink_cache(reader, singleton,
            R"(base\gameplay\gui\common\main_colors.inkstyle)",
            "MainColors.Red");
        log << "Ink style: " << style.reason
            << " | cached_tokens=" << style.cached_tokens
            << " styles=" << style.styles
            << " properties=" << style.properties
            << " style_capacity=" << style.style_capacity
            << " themes=" << style.themes
            << " overrides=" << style.overrides
            << " imports=" << style.imports << "\n";
        // main_colors.inkstyle often owns only theme/override references
        // (the owner's actual alpha.8 log had styles=0, themes=10).
        // Follow already-loaded native Ref<inkStyleResource> dependencies,
        // without issuing new resource requests or modifying any handle.
        if (style.cached_resource_address != 0) {
            const auto graph = inspect_ink_style_graph(
                reader,style.cached_resource_address,
                resource_path_hash(R"(base\gameplay\gui\common\main_colors.inkstyle)"),
                "MainColors.Red");
            log << "Ink linked style graph: " << graph.reason
                << " resources=" << graph.resources_visited
                << " references=" << graph.references_seen
                << " loaded=" << graph.references_loaded
                << " styles=" << graph.styles_visited
                << " properties=" << graph.properties_visited << "\n";
            if (graph.found_color) {
                log << std::fixed << std::setprecision(5)
                    << "Linked MainColors.Red read-only HDRColor (no owning handle): "
                    << graph.color.red << ", " << graph.color.green << ", "
                    << graph.color.blue << ", " << graph.color.alpha << "\n";
                log.unsetf(std::ios::floatfield);
            }
        }
        if (style.status == InkCacheProbe::Status::PropertyCandidate) {
            log << std::fixed << std::setprecision(5)
                << "MainColors.Red read-only RTTI-checked color (no owning handle): "
                << style.rgba[0] << ", " << style.rgba[1] << ", "
                << style.rgba[2] << ", " << style.rgba[3] << "\n";
            log.unsetf(std::ios::floatfield);
        }
    } catch (...) {
        log << "Ink probe encountered an exception; inspection safely stopped\n";
    }
}
} // namespace dreamui
