#include "dreamui/ink_load_patch.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace dreamui {
namespace {
std::uint32_t u32(const std::uint8_t* p,std::size_t off) {
    std::uint32_t x=0;std::memcpy(&x,p+off,sizeof(x));return x;
}
std::uint64_t u64(const std::uint8_t* p,std::size_t off) {
    std::uint64_t x=0;std::memcpy(&x,p+off,sizeof(x));return x;
}
bool read_at(const MemoryRead& r,std::uint64_t addr,std::uint64_t offset,
             void* out,std::size_t n) {
    return addr>=0x10000 && offset<=UINT64_MAX-addr &&
           n<=UINT64_MAX-(addr+offset) && r(addr+offset,out,n);
}
struct Array {
    std::uint64_t ptr=0;
    std::uint32_t capacity=0, size=0;
};
bool array_at(const std::uint8_t* data,std::size_t off,
              std::uint32_t hard_limit,Array& out) {
    out={u64(data,off),u32(data,off+8),u32(data,off+12)};
    return out.size<=hard_limit && out.size<=out.capacity &&
           (!out.size || out.ptr>=0x10000);
}
struct Saved {
    std::uint64_t address=0;
    HdrColor before{};
    HdrColor after{};
};
bool is_color_type(const MemoryRead& r,std::uint64_t type) {
    if(type<0x10000 || (type&1))return false;
    std::array<std::uint8_t,0x78> meta{};
    return read_at(r,type,0,meta.data(),meta.size()) &&
           u64(meta.data(),0x18)==cname_hash("HDRColor") &&
           u32(meta.data(),0x68)==0x10 &&
           u32(meta.data(),0x74)==0x10;
}
bool same_color(const HdrColor& a,const HdrColor& b) {
    return std::memcmp(&a,&b,sizeof(HdrColor))==0;
}
} // namespace

InkLoadPatchResult patch_loaded_ink_color(
    const MemoryRead& read,const MemoryWrite& write,
    std::uint64_t resource,std::uint64_t path,
    std::string_view property,Rgb selected) {
    InkLoadPatchResult out;
    if(!read || !write || resource<0x10000 || !path ||
       property.empty() || property.size()>128)
        return out;
    std::array<std::uint8_t,0x50> header{};
    if(!read_at(read,resource,0,header.data(),header.size()) ||
       u64(header.data(),0x30)!=path)
        return out;

    Array styles{};
    if(!array_at(header.data(),0x40,32,styles))return out;
    if(!styles.size) {out.status=InkLoadPatchStatus::NoStyles;return out;}

    std::array<Saved,32> snapshots{};
    const auto target=cname_hash(property);
    for(std::uint32_t si=0;si<styles.size;++si) {
        std::array<std::uint8_t,0x20> style{};
        if(!read_at(read,styles.ptr,static_cast<std::uint64_t>(si)*0x20,
                    style.data(),style.size()))return out;
        Array properties{};
        if(!array_at(style.data(),0x10,1024,properties))return out;
        for(std::uint32_t pi=0;pi<properties.size;++pi) {
            std::array<std::uint8_t,0x20> item{};
            if(!read_at(read,properties.ptr,static_cast<std::uint64_t>(pi)*0x20,
                        item.data(),item.size()))return out;
            if(u64(item.data(),0)!=target)continue;
            if(out.matched==snapshots.size()) {out.status=InkLoadPatchStatus::InvalidProperty;return out;}
            const auto type=u64(item.data(),0x8);
            const auto address=u64(item.data(),0x10);
            if(!is_color_type(read,type) || address<0x10000 || (address&15)) {
                out.status=InkLoadPatchStatus::InvalidProperty;return out;
            }
            HdrColor current{};
            if(!read_at(read,address,0,&current,sizeof(current)) ||
               !finite_hdr(current)) {
                out.status=InkLoadPatchStatus::InvalidProperty;return out;
            }
            for(std::uint32_t i=0;i<out.matched;++i) {
                if(snapshots[i].address==address) {
                    out.status=InkLoadPatchStatus::InvalidProperty;return out;
                }
            }
            // Preserve the native property's existing HDR peak and alpha.
            // This is an SDR authoring preview, not calibrated HDR transfer.
            const auto peak=std::max({current.red,current.green,current.blue});
            if(!(peak>0.f) || peak>65504.f) {
                out.status=InkLoadPatchStatus::InvalidProperty;return out;
            }
            const float scale=peak/255.0f;
            HdrColor desired{selected.r*scale,selected.g*scale,
                             selected.b*scale,current.alpha};
            if(!finite_hdr(desired)) {
                out.status=InkLoadPatchStatus::InvalidProperty;return out;
            }
            snapshots[out.matched++]={address,current,desired};
        }
    }
    if(out.matched==0) {
        out.status=InkLoadPatchStatus::NoMatchingProperty;return out;
    }
    out.original=snapshots[0].before;
    out.preview=snapshots[0].after;
    bool changed=false;
    for(std::uint32_t i=0;i<out.matched;++i)
        changed |= !same_color(snapshots[i].before,snapshots[i].after);
    if(!changed) {out.status=InkLoadPatchStatus::AlreadyMatches;return out;}

    // Refuse mutations when any target changed while being inspected.
    for(std::uint32_t i=0;i<out.matched;++i) {
        HdrColor current{};
        if(!read_at(read,snapshots[i].address,0,&current,sizeof(current)) ||
           !same_color(current,snapshots[i].before)) {
            out.status=InkLoadPatchStatus::InvalidProperty;return out;
        }
    }

    for(std::uint32_t i=0;i<out.matched;++i) {
        HdrColor current{};
        // Rollback includes the currently attempted write, which may change
        // memory and then return false. Journal is fully prepared beforehand.
        const bool success=write(snapshots[i].address,
                                 &snapshots[i].after,sizeof(HdrColor)) &&
                           read_at(read,snapshots[i].address,0,&current,sizeof(current)) &&
                           same_color(current,snapshots[i].after);
        if(!success) {
            bool restored=true;
            for(std::uint32_t j=0;j<=i;++j) {
                HdrColor restored_value{};
                const auto& old=snapshots[j];
                restored &= write(old.address,&old.before,sizeof(old.before)) &&
                            read_at(read,old.address,0,&restored_value,sizeof(restored_value)) &&
                            same_color(restored_value,old.before);
            }
            out.status=restored ? InkLoadPatchStatus::WriteFailed
                                : InkLoadPatchStatus::RollbackFailed;
            return out;
        }
        ++out.written;
    }
    out.status=InkLoadPatchStatus::Applied;
    return out;
}
}
