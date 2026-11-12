#include "dreamui/pe_probe.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

namespace dreamui {
namespace {
bool in_bounds(std::size_t pos, std::size_t count, std::size_t total) {
    return pos <= total && count <= total - pos;
}
std::uint16_t u16(std::span<const std::uint8_t> bytes, std::size_t p) {
    return static_cast<std::uint16_t>(bytes[p] | (bytes[p+1] << 8));
}
std::uint32_t u32(std::span<const std::uint8_t> bytes, std::size_t p) {
    return static_cast<std::uint32_t>(bytes[p]) |
           (static_cast<std::uint32_t>(bytes[p+1]) << 8) |
           (static_cast<std::uint32_t>(bytes[p+2]) << 16) |
           (static_cast<std::uint32_t>(bytes[p+3]) << 24);
}
bool contains(std::span<const std::uint8_t> bytes, std::string_view needle) {
    const auto begin = reinterpret_cast<const std::uint8_t*>(needle.data());
    return std::search(bytes.begin(), bytes.end(), begin, begin + needle.size()) != bytes.end();
}
}
PeProbe inspect_pe(std::span<const std::uint8_t> image) {
    PeProbe result;
    if (!in_bounds(0,64,image.size()) || u16(image,0)!=0x5A4D) {
        result.error="missing DOS header"; return result;
    }
    const auto pe=static_cast<std::size_t>(u32(image,0x3C));
    if (!in_bounds(pe,24,image.size()) || u32(image,pe)!=0x00004550) {
        result.error="missing PE header"; return result;
    }
    const auto machine=u16(image,pe+4);
    const auto sections=u16(image,pe+6);
    const auto opt_size=u16(image,pe+20);
    const auto opt=pe+24;
    if (machine!=0x8664 || sections==0 || sections>96 || opt_size<60 ||
        !in_bounds(opt,opt_size,image.size()) || u16(image,opt)!=0x20B) {
        result.error="unsupported or malformed PE32+ image"; return result;
    }
    const auto section_table=opt+opt_size;
    if (!in_bounds(section_table, static_cast<std::size_t>(sections)*40,image.size())) {
        result.error="truncated PE section table"; return result;
    }
    result.x64=true;
    result.coff_timestamp=u32(image,pe+8);
    result.image_size=u32(image,opt+56);
    result.sections.image_size=result.image_size;
    // Inspect only the on-disk .rdata section: no scanning live game memory.
    // Literal string hits are diagnostic clues, not validated engine addresses.
    for (std::size_t i=0;i<sections;i++) {
        const auto entry=section_table+i*40;
        const auto name=std::string_view(reinterpret_cast<const char*>(image.data()+entry),8);
        const auto virtual_rva=u32(image,entry+12);
        const auto virtual_bytes=std::max(u32(image,entry+8),u32(image,entry+16));
        const auto bounded_bytes=(virtual_rva<result.image_size)
            ? std::min(virtual_bytes,result.image_size-virtual_rva):0u;
        if(name.substr(0,5)==".text" && bounded_bytes) {
            result.sections.text_rva=virtual_rva;
            result.sections.text_size=bounded_bytes;
        } else if(name.substr(0,5)==".data" && bounded_bytes) {
            result.sections.data_rva=virtual_rva;
            result.sections.data_size=bounded_bytes;
        } else if(name.substr(0,6)==".rdata" && bounded_bytes) {
            result.sections.rdata_rva=virtual_rva;
            result.sections.rdata_size=bounded_bytes;
        }
        constexpr std::array<char,8> rdata={'.','r','d','a','t','a',0,0};
        if (std::memcmp(image.data()+entry,rdata.data(),rdata.size())!=0) continue;
        const auto size=static_cast<std::size_t>(u32(image,entry+16));
        const auto offset=static_cast<std::size_t>(u32(image,entry+20));
        if (size>64*1024*1024 || !in_bounds(offset,size,image.size())) {
            result.error="invalid .rdata section bounds"; return result;
        }
        const auto data=image.subspan(offset,size);
        result.rdata_bytes_scanned+=size;
        result.ink_style_resource_literal |= contains(data,"inkStyleResource");
        result.ink_style_property_literal |= contains(data,"inkStyleProperty");
        result.ink_widget_literal |= contains(data,"inkWidget");
    }
    result.valid=true;
    return result;
}
} // namespace dreamui
