#include "dreamui/ink_graph.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <vector>

namespace dreamui {
namespace {
std::uint32_t u32(const std::uint8_t* b,std::size_t offset) {
    std::uint32_t value=0;
    std::memcpy(&value,b+offset,sizeof(value));
    return value;
}
std::uint64_t u64(const std::uint8_t* b,std::size_t offset) {
    std::uint64_t value=0;
    std::memcpy(&value,b+offset,sizeof(value));
    return value;
}
bool read_at(const MemoryRead& read,std::uint64_t base,
             std::uint64_t offset,void* out,std::size_t count) {
    return base>=0x10000 && offset<=UINT64_MAX-base &&
        count<=UINT64_MAX-(base+offset) && read(base+offset,out,count);
}
struct Pending {
    std::uint64_t address=0, path_hash=0;
    unsigned depth=0;
};
struct NativeArray {
    std::uint64_t entries=0;
    std::uint32_t capacity=0, size=0;
};
bool parse_array(const std::uint8_t* b,std::size_t off,
                 std::uint32_t limit,NativeArray& out) {
    out={u64(b,off),u32(b,off+8),u32(b,off+12)};
    return out.size<=limit && out.size<=out.capacity &&
        (!out.size || out.entries>=0x10000);
}
bool read_hdr_property(const MemoryRead& read,
                       const std::uint8_t* item,
                       HdrColor& color) {
    const auto tag=u64(item,8);
    if(tag<0x10000 || (tag&1)!=0) return false;
    // NativeTypes-inl.hpp: HDRColor alignment 16 > Variant::InlineAlignment 8.
    std::array<std::uint8_t,0x78> type{};
    if(!read_at(read,tag,0,type.data(),type.size())) return false;
    if(u64(type.data(),0x18)!=cname_hash("HDRColor") ||
       u32(type.data(),0x68)!=16 || u32(type.data(),0x74)!=16) return false;
    const auto data=u64(item,16);
    if(data<0x10000 || (data&0xF)!=0) return false;
    return read_at(read,data,0,&color,sizeof(color)) && finite_hdr(color);
}
}
InkStyleGraphProbe inspect_ink_style_graph(
    const MemoryRead& read,std::uint64_t root_resource,
    std::uint64_t expected_root_path_hash,std::string_view target_property) {
    InkStyleGraphProbe out;
    if(!read || root_resource<0x10000 || !expected_root_path_hash ||
       target_property.empty() || target_property.size()>128) {
        out.reason="invalid graph request";return out;
    }
    const auto target_hash=cname_hash(target_property);
    std::vector<Pending> todo{{root_resource,expected_root_path_hash,0}};
    std::set<std::uint64_t> visited;

    while(!todo.empty()) {
        const auto next=todo.back();
        todo.pop_back();
        if(!visited.insert(next.address).second)continue;
        if(visited.size()>64){out.reason="Ink resource graph exceeds 64 nodes";return out;}
        std::array<std::uint8_t,0x80> header{};
        if(!read_at(read,next.address,0,header.data(),header.size())) {
            out.reason="Ink graph resource header unreadable";return out;
        }
        if(u64(header.data(),0x30)!=next.path_hash) {
            out.reason="Ink graph resource/path identity mismatch";return out;
        }
        ++out.resources_visited;
        NativeArray styles{},themes{},overrides{},imports{};
        if(!parse_array(header.data(),0x40,512,styles) ||
           !parse_array(header.data(),0x50,1024,themes) ||
           !parse_array(header.data(),0x60,1024,overrides) ||
           !parse_array(header.data(),0x70,1024,imports)) {
            out.reason="Ink graph array bounds invalid";return out;
        }

        for(std::uint32_t si=0;si<styles.size && si<32;++si) {
            std::array<std::uint8_t,0x20> style{};
            if(!read_at(read,styles.entries,static_cast<std::uint64_t>(si)*0x20,
                        style.data(),style.size())) {
                out.reason="Ink graph style unreadable";return out;
            }
            ++out.styles_visited;
            NativeArray properties{};
            if(!parse_array(style.data(),0x10,4096,properties)) {
                out.reason="Ink graph property bounds invalid";return out;
            }
            for(std::uint32_t pi=0;pi<properties.size && pi<512;++pi) {
                std::array<std::uint8_t,0x20> item{};
                if(!read_at(read,properties.entries,static_cast<std::uint64_t>(pi)*0x20,
                            item.data(),item.size())) {
                    out.reason="Ink graph property unreadable";return out;
                }
                ++out.properties_visited;
                if(u64(item.data(),0)!=target_hash)continue;
                if(!read_hdr_property(read,item.data(),out.color)) {
                    out.reason="Ink graph matching property has invalid HDRColor Variant";
                    return out;
                }
                out.valid=true;
                out.found_color=true;
                out.owner_path_hash=next.path_hash;
                out.reason="read-only HDRColor in loaded linked inkStyleResource";
                return out;
            }
        }

        // Only follow known native Ref<inkStyleResource> fields. Each ref is
        // 0x18 bytes: ResourcePath hash +0, token instance +8, refcount +16.
        if(next.depth>=4)continue;
        for(const auto& [arr,stride]:std::array<std::pair<NativeArray,unsigned>,3>{
            {{themes,0x20},{overrides,0x20},{imports,0x18}}}) {
            for(std::uint32_t ri=0;ri<arr.size && ri<128;++ri) {
                ++out.references_seen;
                std::array<std::uint8_t,0x18> ref{};
                if(!read_at(read,arr.entries,static_cast<std::uint64_t>(ri)*stride,
                            ref.data(),ref.size())) {
                    out.reason="Ink graph resource reference unreadable";return out;
                }
                const auto path_hash=u64(ref.data(),0);
                const auto token=u64(ref.data(),8);
                const auto refcount=u64(ref.data(),16);
                if(!path_hash || token<0x10000 || refcount<0x10000)continue;
                std::array<std::uint8_t,8> counts{};
                if(!read_at(read,refcount,0,counts.data(),counts.size()))continue;
                if(u32(counts.data(),0)==0)continue;
                std::array<std::uint8_t,0x60> token_header{};
                if(!read_at(read,token,0,token_header.data(),token_header.size()))continue;
                if(u64(token_header.data(),0x48)!=path_hash ||
                   !u32(token_header.data(),0x58) || token_header[0x5c])continue;
                const auto resource=u64(token_header.data(),0x28);
                if(resource<0x10000)continue;
                ++out.references_loaded;
                if(todo.size()>=64) {
                    out.reason="Ink graph pending list exceeds 64 nodes";return out;
                }
                todo.push_back({resource,path_hash,next.depth+1});
            }
        }
    }
    out.valid=true;
    out.reason="no matching HDRColor found in bounded loaded style graph";
    return out;
}
} // namespace dreamui
