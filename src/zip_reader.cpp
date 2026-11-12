#include "dreamui/zip_reader.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <unordered_set>

namespace dreamui {
namespace {
constexpr std::size_t kMaxPackage = 16 * 1024 * 1024;
constexpr std::size_t kMaxEntries = 128;
std::uint16_t u16(const std::vector<std::uint8_t>& d, std::size_t p) {
    return static_cast<std::uint16_t>(d[p] | (d[p+1] << 8));
}
std::uint32_t u32(const std::vector<std::uint8_t>& d, std::size_t p) {
    return static_cast<std::uint32_t>(d[p]) | (static_cast<std::uint32_t>(d[p+1]) << 8) |
           (static_cast<std::uint32_t>(d[p+2]) << 16) | (static_cast<std::uint32_t>(d[p+3]) << 24);
}
bool within(std::size_t p, std::size_t n, std::size_t size) { return p <= size && n <= size - p; }
std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
    std::uint32_t crc = 0xffffffff;
    for (std::size_t i=0; i<len; ++i) {
        crc ^= data[i];
        for (int bit=0; bit<8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
bool safe_path(std::string_view path) {
    if (path.empty() || path.front()=='/' || path.find('\\')!=path.npos || path.find(':')!=path.npos || path.find('\0')!=path.npos) return false;
    std::size_t pos=0;
    while (pos < path.size()) {
        const auto end = path.find('/', pos);
        auto component = path.substr(pos, end == path.npos ? path.npos : end-pos);
        if (component.empty() || component=="." || component=="..") return false;
        if (end == path.npos) break;
        pos = end+1;
    }
    return true;
}
}
const PackageEntry* PackageResult::find(std::string_view name) const {
    for (const auto& e : entries) if (e.name == name) return &e;
    return nullptr;
}
PackageResult read_dream_package(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {{}, "cannot open .dream package"};
    const auto length = in.tellg();
    if (length < 22 || length > static_cast<std::streamoff>(kMaxPackage)) return {{}, "package size outside allowed limits"};
    in.seekg(0);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    if (!in.read(reinterpret_cast<char*>(data.data()), length)) return {{}, "cannot read package"};
    // End of central directory: ZIP comment can contain up to 65535 bytes.
    std::optional<std::size_t> eocd;
    for (std::size_t p=data.size()-22;;--p) {
        if (u32(data,p)==0x06054b50u && within(p, 22, data.size()) && within(p+22,u16(data,p+20),data.size()) && p+22+u16(data,p+20)==data.size()) { eocd=p; break; }
        if (p==0 || data.size()-p>65557) break;
    }
    if (!eocd) return {{}, "ZIP central directory not found"};
    const auto pos=*eocd;
    if (u16(data,pos+4)!=0 || u16(data,pos+6)!=0 || u16(data,pos+8)!=u16(data,pos+10)) return {{}, "multi-disk ZIP unsupported"};
    const auto count=u16(data,pos+10);
    const auto dirSize=u32(data,pos+12), dirOffset=u32(data,pos+16);
    if (count==0 || count>kMaxEntries || !within(dirOffset,dirSize,pos)) return {{}, "invalid ZIP directory"};
    PackageResult result;
    std::unordered_set<std::string> seen;
    std::size_t cursor=dirOffset;
    for (std::size_t i=0;i<count;++i) {
        if (!within(cursor,46,pos) || u32(data,cursor)!=0x02014b50u) return {{}, "invalid central directory entry"};
        const auto flags=u16(data,cursor+8), method=u16(data,cursor+10);
        const auto storedCrc=u32(data,cursor+16), compressed=u32(data,cursor+20), original=u32(data,cursor+24);
        const auto nameLen=u16(data,cursor+28), extraLen=u16(data,cursor+30), commentLen=u16(data,cursor+32);
        const auto localOffset=u32(data,cursor+42);
        const std::size_t total=46ull+nameLen+extraLen+commentLen;
        if (!within(cursor,total,pos) || (flags & 1) || method!=0 || compressed!=original || original>kMaxPackage) return {{}, "unsupported or malformed ZIP entry"};
        std::string name(reinterpret_cast<const char*>(data.data()+cursor+46),nameLen);
        if (!safe_path(name) || !seen.insert(name).second) return {{}, "unsafe or duplicate ZIP path"};
        if (!within(localOffset,30,data.size()) || u32(data,localOffset)!=0x04034b50u) return {{}, "invalid local ZIP header"};
        const std::size_t payload=static_cast<std::size_t>(localOffset)+30+u16(data,localOffset+26)+u16(data,localOffset+28);
        if (!within(payload,compressed,dirOffset)) return {{}, "ZIP data outside expected section"};
        if (crc32(data.data()+payload,original)!=storedCrc) return {{}, "ZIP CRC mismatch"};
        result.entries.push_back({std::move(name),{data.begin()+payload,data.begin()+payload+original}});
        cursor+=total;
    }
    if (cursor!=static_cast<std::size_t>(dirOffset)+dirSize) return {{}, "ZIP directory size mismatch"};
    if (!result.find("manifest.json")) return {{}, "missing manifest.json"};
    return result;
}
}
