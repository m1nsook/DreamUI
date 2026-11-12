#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include "dreamui/win_probe.hpp"
#include "dreamui/pe_probe.hpp"
#include "dreamui/address_db.hpp"
#include <cstdint>
#include <array>
#include <filesystem>
#include <ios>
#include <ostream>
#include <span>
#include <vector>

namespace dreamui {
namespace {
struct FileHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~FileHandle() { if (handle!=INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
struct MappingHandle {
    HANDLE handle = nullptr;
    ~MappingHandle() { if (handle) CloseHandle(handle); }
};
struct View {
    const void* address = nullptr;
    ~View() { if (address) UnmapViewOfFile(address); }
};
}
void log_executable_probe(const std::filesystem::path& executable, std::ostream& log) noexcept {
    try {
        log << "Engine probe: read-only executable metadata, no in-memory scanning\n";
        DWORD ignored = 0;
        const auto resource_size = GetFileVersionInfoSizeW(executable.c_str(),&ignored);
        if (resource_size && resource_size <= 1024*1024) {
            std::vector<std::uint8_t> info(resource_size);
            if (GetFileVersionInfoW(executable.c_str(),0,resource_size,info.data())) {
                void* value = nullptr; UINT length=0;
                if (VerQueryValueW(info.data(),L"\\",&value,&length) &&
                    value && length >= sizeof(VS_FIXEDFILEINFO)) {
                    auto* fixed = static_cast<const VS_FIXEDFILEINFO*>(value);
                    if (fixed->dwSignature==0xFEEF04BD) {
                        log << "Executable file version: "
                            << HIWORD(fixed->dwFileVersionMS) << '.'
                            << LOWORD(fixed->dwFileVersionMS) << '.'
                            << HIWORD(fixed->dwFileVersionLS) << '.'
                            << LOWORD(fixed->dwFileVersionLS) << " (file resource)\n";
                    }
                }
            }
        }
        FileHandle file{CreateFileW(executable.c_str(),GENERIC_READ,
                                   FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                                   nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if (file.handle==INVALID_HANDLE_VALUE) { log << "Engine probe: executable cannot be opened\n"; return; }
        LARGE_INTEGER length{};
        if (!GetFileSizeEx(file.handle,&length) || length.QuadPart<64 || length.QuadPart>1024LL*1024*1024) {
            log << "Engine probe: executable outside supported file size\n";return;
        }
        MappingHandle mapping{CreateFileMappingW(file.handle,nullptr,PAGE_READONLY,0,0,nullptr)};
        if (!mapping.handle) { log << "Engine probe: read-only file mapping failed\n";return; }
        View view{MapViewOfFile(mapping.handle,FILE_MAP_READ,0,0,0)};
        if (!view.address) { log << "Engine probe: read-only view failed\n";return; }
        const auto bytes=std::span(reinterpret_cast<const std::uint8_t*>(view.address),
                                   static_cast<std::size_t>(length.QuadPart));
        const auto result=inspect_pe(bytes);
        if (!result.valid) { log << "Engine probe: invalid executable PE: " << result.error << '\n';return; }
        log << "Engine PE: x64=" << result.x64 << " timestamp=" << result.coff_timestamp
            << " image_size=" << result.image_size
            << " rdata_scanned=" << result.rdata_bytes_scanned << " bytes\n";
        log << "Engine literals (diagnostic only): inkStyleResource=" << result.ink_style_resource_literal
            << " inkStyleProperty=" << result.ink_style_property_literal
            << " inkWidget=" << result.ink_widget_literal << "\n";
        // Resolve only four known hashes using the game-provided address
        // database. No external resolver DLL is loaded or called.
        constexpr std::uint32_t kResourceLoader=0x783C1034;
        constexpr std::array<std::uint32_t,4> kHashes = {
            kResourceLoader, 0xC86F39B7, 0x4A610F64, 0x9D14104F
        };
        const auto db=read_address_database(
            executable.parent_path() / L"cyberpunk2077_addresses.json",
            result.coff_timestamp,result.sections,kHashes);
        if(!db.valid) {
            log << "Address DB: rejected (" << db.error << "); native probe disabled\n";
        } else {
            log << "Address DB: game linker map validated, entries_scanned=" << db.entries_scanned
                << " supported_symbols=" << db.matches.size() << '/' << kHashes.size() << "\n";
            for(auto hash:kHashes) {
                const auto it=db.matches.find(hash);
                if(it==db.matches.end()) {
                    log << "Engine symbol hash=" << hash << ": missing\n";
                } else {
                    log << "Engine symbol hash=" << hash << ": segment=" << it->second.segment
                        << " rva=" << it->second.rva << " (address only, not invoked)\n";
                }
            }
            const auto it=db.matches.find(kResourceLoader);
            if(it!=db.matches.end() && it->second.segment==3 &&
               it->second.rva<result.image_size &&
               result.image_size-it->second.rva>=sizeof(void*)) {
                const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
                const auto location=reinterpret_cast<const void*>(base+it->second.rva);
                std::uintptr_t singleton=0;
                SIZE_T read=0;
                if(ReadProcessMemory(GetCurrentProcess(),location,&singleton,sizeof(singleton),&read) &&
                   read==sizeof(singleton)) {
                    MEMORY_BASIC_INFORMATION info{};
                    bool plausible=false;
                    if(singleton>=0x10000 && VirtualQuery(reinterpret_cast<void*>(singleton),&info,sizeof(info))==sizeof(info)) {
                        const auto protection=info.Protect & 0xff;
                        plausible=info.State==MEM_COMMIT && !(info.Protect & PAGE_GUARD) &&
                            protection!=PAGE_NOACCESS;
                    }
                    log << "ResourceLoader singleton: "
                        << (singleton==0 ? "null/not initialized" : plausible ? "pointer present (not dereferenced)" : "unverified pointer")
                        << "\n";
                } else {
                    log << "ResourceLoader singleton: safe read failed\n";
                }
            } else {
                log << "ResourceLoader singleton: no validated data-segment symbol\n";
            }
        }
        log << "Engine status: no live Ink resource handles acquired; game memory untouched\n";
    } catch (...) {
        log << "Engine probe encountered an error and safely stopped\n";
    }
}
} // namespace dreamui
