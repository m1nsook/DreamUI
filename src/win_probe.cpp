#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include "dreamui/win_probe.hpp"
#include "dreamui/pe_probe.hpp"
#include "dreamui/address_db.hpp"
#include "dreamui/ink_cache.hpp"
#include <cstdint>
#include <iomanip>
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
                // ASI bootstrap can run before the game's resource loader is
                // initialized. Sample the validated global pointer a few times
                // on this background worker thread, with a fixed 15s deadline.
                // No game API calls, object dereferences, or memory writes.
                constexpr std::array<unsigned,4> kSampleTimesMs={0,3000,8000,15000};
                log << "ResourceLoader: checking startup initialization at +0,+3,+8,+15 seconds\n";
                log.flush();
                for(std::size_t attempt=0;attempt<kSampleTimesMs.size();++attempt) {
                    if(attempt>0)
                        Sleep(kSampleTimesMs[attempt]-kSampleTimesMs[attempt-1]);
                    std::uintptr_t singleton=0;
                    SIZE_T read=0;
                    if(!ReadProcessMemory(GetCurrentProcess(),location,&singleton,sizeof(singleton),&read) ||
                       read!=sizeof(singleton)) {
                        log << "ResourceLoader sample +" << kSampleTimesMs[attempt]/1000
                            << "s: safe read failed\n";
                        log.flush();
                        break;
                    }
                    MEMORY_BASIC_INFORMATION info{};
                    bool plausible=false;
                    if(singleton>=0x10000 &&
                       VirtualQuery(reinterpret_cast<const void*>(singleton),&info,sizeof(info))==sizeof(info)) {
                        const auto protection=info.Protect & 0xff;
                        plausible=info.State==MEM_COMMIT && !(info.Protect & PAGE_GUARD) &&
                            protection!=PAGE_NOACCESS;
                    }
                    const char* status=singleton==0 ? "null/not initialized" :
                        plausible ? "pointer present (not dereferenced)" : "unverified pointer";
                    log << "ResourceLoader sample +" << kSampleTimesMs[attempt]/1000
                        << "s: " << status << "\n";
                    log.flush();
                    if(plausible) {
                        // Read-only inspection of the ALREADY-CACHED vanilla
                        // resource; supported only on the owner's verified
                        // executable fingerprint. No native game calls.
                        if(result.coff_timestamp==1792368047u && result.image_size==92385280u) {
                            const MemoryRead reader=[](std::uint64_t address,void* destination,std::size_t size) {
                                if(address<0x10000 || size==0 || size>4096 ||
                                   address>UINT64_MAX-size)return false;
                                SIZE_T copied=0;
                                return ReadProcessMemory(GetCurrentProcess(),
                                    reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
                                    destination,size,&copied) && copied==size;
                            };
                            constexpr std::array<unsigned,3> kCacheDelaySeconds={0,5,12};
                            for(std::size_t phase=0;phase<kCacheDelaySeconds.size();++phase) {
                                if(phase>0)
                                    Sleep((kCacheDelaySeconds[phase]-kCacheDelaySeconds[phase-1])*1000);
                                const auto style=inspect_ink_cache(reader,singleton,
                                    R"(base\gameplay\gui\common\main_colors.inkstyle)",
                                    "MainColors.PanelWhite");
                                log << "Ink style cache check +" << kCacheDelaySeconds[phase]
                                    << "s after loader initialization: " << style.reason
                                    << " | cached_tokens=" << style.cached_tokens
                                    << " styles=" << style.styles
                                    << " properties=" << style.properties << "\n";
                                if(style.status==InkCacheProbe::Status::PropertyCandidate) {
                                    log << std::fixed << std::setprecision(5);
                                    log << "Ink MainColors.PanelWhite candidate RGBA (unverified RTTI type): "
                                        << style.rgba[0] << ", " << style.rgba[1] << ", "
                                        << style.rgba[2] << ", " << style.rgba[3] << "\n";
                                    log.unsetf(std::ios::floatfield);
                                }
                                log.flush();
                                // Stop once the cache contains a native style,
                                // even when the color property is not present.
                                if(style.found_token || style.status==InkCacheProbe::Status::Invalid)
                                    break;
                            }
                        } else {
                            log << "Ink style cache: unsupported executable fingerprint; read disabled\n";
                        }
                        break;
                    }
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
