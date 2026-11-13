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
    std::uint64_t type=0,property_hash=0;
    std::uint32_t style_index=0,property_index=0;
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

InkLoadPatchResult patch_loaded_ink_palette(
    const MemoryRead& read,const MemoryWrite& write,
    std::uint64_t resource,std::uint64_t path,
    std::span<const InkPaletteEntry> targets,
    InkLoadJournal* journal) {
    InkLoadPatchResult out;
    if(journal && journal->active()) {
        out.status=InkLoadPatchStatus::JournalOccupied;return out;
    }
    if(!read || !write || resource<0x10000 || !path ||
       targets.empty() || targets.size()>16)
        return out;
    std::array<std::uint64_t,16> hashes{};
    std::array<bool,16> found{};
    for(std::size_t i=0;i<targets.size();++i) {
        const auto property=targets[i].native_property;
        if(property.empty() || property.size()>128)return out;
        const auto hash=cname_hash(property);
        for(std::size_t j=0;j<i;++j)if(hashes[j]==hash)return out;
        hashes[i]=hash;
    }
    std::array<std::uint8_t,0x50> header{};
    if(!read_at(read,resource,0,header.data(),header.size()) ||
       u64(header.data(),0x30)!=path)
        return out;

    const auto resource_id=u64(header.data(),0x28);
    if(journal && !resource_id)return out; // verified serializer ID required
    Array styles{};
    if(!array_at(header.data(),0x40,32,styles))return out;
    if(!styles.size) {out.status=InkLoadPatchStatus::NoStyles;return out;}

    std::array<Saved,32> snapshots{};
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
            const auto property_hash=u64(item.data(),0);
            auto hit=std::find(hashes.begin(),hashes.begin()+targets.size(),property_hash);
            if(hit==hashes.begin()+targets.size())continue;
            const auto index=static_cast<std::size_t>(hit-hashes.begin());
            found[index]=true;
            const Rgb selected=targets[index].theme_color;
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
            snapshots[out.matched++]={address,current,desired,type,property_hash,si,pi};
        }
    }
    for(std::size_t i=0;i<targets.size();++i) {
        if(!found[i]) {
            out.status=InkLoadPatchStatus::NoMatchingProperty;return out;
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
    if(journal) {
        journal->resource_address=resource;
        journal->resource_path_hash=path;
        journal->resource_instance_id=resource_id;
        journal->count=out.matched;
        for(std::uint32_t i=0;i<out.matched;++i) {
            const auto& e=snapshots[i];
            journal->entries[i]={
                e.address,e.type,e.property_hash,e.style_index,e.property_index,e.before,e.after
            };
        }
    }
    out.status=InkLoadPatchStatus::Applied;
    return out;
}

InkLoadPatchResult patch_loaded_ink_color(
    const MemoryRead& read,const MemoryWrite& write,
    std::uint64_t resource,std::uint64_t path,
    std::string_view property,Rgb selected,InkLoadJournal* journal) {
    const std::array<InkPaletteEntry,1> one{{{property,selected}}};
    return patch_loaded_ink_palette(read,write,resource,path,one,journal);
}

// Do not invoke outside an authenticated engine-owned callback. This
// function cannot acquire ownership or make a raw pointer safe by itself.
InkLoadRestoreStatus restore_loaded_ink_color(
    const MemoryRead& read,const MemoryWrite& write,
    std::uint64_t resource,const InkLoadJournal& journal) {
    if(!journal.active())return InkLoadRestoreStatus::NoJournal;
    if(!read || !write || resource<0x10000)
        return InkLoadRestoreStatus::InvalidResource;
    if(resource!=journal.resource_address ||
       !journal.resource_path_hash || !journal.resource_instance_id ||
       journal.count>journal.entries.size())
        return InkLoadRestoreStatus::StaleResource;
    std::array<std::uint8_t,0x50> header{};
    if(!read_at(read,resource,0,header.data(),header.size()) ||
       u64(header.data(),0x30)!=journal.resource_path_hash ||
       u64(header.data(),0x28)!=journal.resource_instance_id)
        return InkLoadRestoreStatus::StaleResource;
    Array styles{};
    if(!array_at(header.data(),0x40,32,styles))
        return InkLoadRestoreStatus::InvalidResource;

    // Complete validation before the first native write. If any property
    // changed after we patched it, don't overwrite another mod's work.
    for(std::uint32_t i=0;i<journal.count && i<journal.entries.size();++i) {
        const auto& entry=journal.entries[i];
        if(entry.style_index>=styles.size)return InkLoadRestoreStatus::ChangedExternally;
        std::array<std::uint8_t,0x20> style{};
        if(!read_at(read,styles.ptr,std::uint64_t(entry.style_index)*0x20,
                    style.data(),style.size()))
            return InkLoadRestoreStatus::InvalidResource;
        Array props{};
        if(!array_at(style.data(),0x10,1024,props))
            return InkLoadRestoreStatus::InvalidResource;
        if(entry.property_index>=props.size)
            return InkLoadRestoreStatus::ChangedExternally;
        std::array<std::uint8_t,0x20> item{};
        if(!read_at(read,props.ptr,std::uint64_t(entry.property_index)*0x20,
                    item.data(),item.size()))
            return InkLoadRestoreStatus::InvalidResource;
        if(u64(item.data(),0)!=entry.property_path_hash ||
           u64(item.data(),8)!=entry.type_address ||
           u64(item.data(),16)!=entry.color_address ||
           !is_color_type(read,entry.type_address))
            return InkLoadRestoreStatus::ChangedExternally;
        HdrColor current{};
        if(!read_at(read,entry.color_address,0,&current,sizeof(current)) ||
           !same_color(current,entry.applied))
            return InkLoadRestoreStatus::ChangedExternally;
    }

    for(std::uint32_t i=0;i<journal.count;++i) {
        const auto& e=journal.entries[i];
        HdrColor current{};
        if(!write(e.color_address,&e.before,sizeof(e.before)) ||
           !read_at(read,e.color_address,0,&current,sizeof(current)) ||
           !same_color(current,e.before)) {
            // Bring all previously touched slots back to our applied state,
            // to keep the journal coherent and permit a later restore retry.
            bool rollback_ok=true;
            for(std::uint32_t j=0;j<=i;++j) {
                const auto& prev=journal.entries[j];
                HdrColor check{};
                if(!write(prev.color_address,&prev.applied,sizeof(prev.applied)) ||
                   !read_at(read,prev.color_address,0,&check,sizeof(check)) ||
                   !same_color(check,prev.applied))
                    rollback_ok=false;
            }
            return rollback_ok ? InkLoadRestoreStatus::WriteFailed
                               : InkLoadRestoreStatus::RollbackFailed;
        }
    }
    return InkLoadRestoreStatus::Restored;
}
}
