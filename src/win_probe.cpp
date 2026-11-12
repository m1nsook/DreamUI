#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include "dreamui/win_probe.hpp"
#include "dreamui/pe_probe.hpp"
#include <cstdint>
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
        log << "Engine status: no live Ink resource handles acquired; game memory untouched\n";
    } catch (...) {
        log << "Engine probe encountered an error and safely stopped\n";
    }
}
} // namespace dreamui
