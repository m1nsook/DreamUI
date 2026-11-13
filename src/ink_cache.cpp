#include "dreamui/ink_cache.hpp"
#include "dreamui/native_ink.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace dreamui {
namespace {
bool add(std::uint64_t base,std::uint64_t offset,std::uint64_t& out) {
    if(base<0x10000 || offset>UINT64_MAX-base) return false;
    out=base+offset;return true;
}
bool read_range(const MemoryRead& reader,std::uint64_t base,std::uint64_t offset,void* data,std::size_t size) {
    std::uint64_t address=0;
    return add(base,offset,address) && reader(address,data,size);
}
std::uint32_t u32(const std::uint8_t* b,std::size_t offset){
    std::uint32_t x=0;std::memcpy(&x,b+offset,4);return x;
}
std::uint64_t u64(const std::uint8_t* b,std::size_t offset){
    std::uint64_t x=0;std::memcpy(&x,b+offset,8);return x;
}
std::uint64_t fnv64(std::string_view name) {
    std::uint64_t hash=0xcbf29ce484222325ULL;
    for(unsigned char c:name){hash^=c;hash*=0x100000001b3ULL;}
    return hash;
}
InkCacheProbe failure(std::string reason){
    InkCacheProbe result;
    result.reason=std::move(reason);
    return result;
}
}
std::uint64_t cname_hash(std::string_view name) { return fnv64(name); }
std::uint64_t resource_path_hash(std::string_view path) {
    if(path.empty() || path.size()>216)return 0;
    if(path.front()=='"' || path.front()=='\'')path.remove_prefix(1);
    while(!path.empty() && (path.front()=='/' || path.front()=='\\'))path.remove_prefix(1);
    std::string sanitized;
    sanitized.reserve(path.size());
    for(char c:path){
        if(c=='"' || c=='\'')break;
        if(c=='/' || c=='\\'){
            if(sanitized.empty() || sanitized.back()=='\\')continue;
            sanitized.push_back('\\');
        } else {
            if(c>='A'&&c<='Z')c=static_cast<char>(c+('a'-'A'));
            sanitized.push_back(c);
        }
    }
    if(sanitized.empty())return 0;
    return fnv64(sanitized);
}
InkCacheProbe inspect_ink_cache(const MemoryRead& read,
                                 std::uint64_t loader,
                                 std::string_view path,
                                 std::string_view property) {
    const auto key=resource_path_hash(path);
    const auto property_hash=cname_hash(property);
    if(!key || property.empty() || !read)return failure("invalid path or memory reader");
    // RED4ext.SDK: ResourceLoader::tokens is HashMap<ResourcePath,WeakPtr<Token>>
    // at +0x00, with a 0x30-byte header. We only COPY its bytes.
    std::array<std::uint8_t,0x30> map{};
    if(!read_range(read,loader,0,map.data(),map.size()))return failure("resource loader header unreadable");
    const auto index=u64(map.data(),0);
    const auto count=u32(map.data(),8);
    const auto capacity=u32(map.data(),12);
    const auto nodes=u64(map.data(),16);
    const auto node_capacity=u32(map.data(),24), stride=u32(map.data(),28),
               node_count=u32(map.data(),36);
    InkCacheProbe out;
    out.cached_tokens=count;
    if(count==0 && capacity==0){out.status=InkCacheProbe::Status::EmptyCache;out.reason="token map not initialized";return out;}
    if(capacity==0 || capacity>4'000'000 || count>capacity || node_capacity<capacity ||
       node_capacity>4'000'000 || node_count>node_capacity || stride!=0x20 ||
       index<0x10000 || nodes<0x10000)
        return failure("resource token hash map metadata failed validation");
    const std::uint32_t hash32=static_cast<std::uint32_t>(key)^static_cast<std::uint32_t>(key>>32);
    const auto bucket=static_cast<std::uint64_t>(hash32%capacity)*4;
    std::uint32_t at=UINT32_MAX;
    if(!read_range(read,index,bucket,&at,sizeof(at)))return failure("token bucket unreadable");
    for(std::uint32_t visited=0;at!=UINT32_MAX && visited<128;++visited){
        if(at>=node_capacity)return failure("token chain index outside capacity");
        std::array<std::uint8_t,0x20> node{};
        if(!read_range(read,nodes,static_cast<std::uint64_t>(at)*stride,node.data(),node.size()))
            return failure("token node unreadable");
        if(u32(node.data(),4)==hash32 && u64(node.data(),8)==key) {
            out.found_token=true;
            const auto token=u64(node.data(),16), refs=u64(node.data(),24);
            if(token<0x10000 || refs<0x10000){out.status=InkCacheProbe::Status::ExpiredToken;out.reason="cached token weak reference is empty";return out;}
            // Weak token may expire concurrently; all reads are guarded and a
            // failure safely terminates observation without dereferencing it.
            std::array<std::uint8_t,8> refcounts{};
            if(!read_range(read,refs,0,refcounts.data(),refcounts.size()))
                return failure("cached token refcount unreadable");
            if(u32(refcounts.data(),0)==0){out.status=InkCacheProbe::Status::ExpiredToken;out.reason="cached token has zero strong references";return out;}
            std::array<std::uint8_t,0x60> token_header{};
            if(!read_range(read,token,0,token_header.data(),token_header.size()))
                return failure("cached token header unreadable");
            if(u64(token_header.data(),0x48)!=key)return failure("cached token resource path mismatch");
            const auto finished=u32(token_header.data(),0x58);
            const auto error=token_header[0x5c];
            if(error){out.status=InkCacheProbe::Status::TokenFailed;out.reason="resource token marked failed";return out;}
            if(!finished){out.status=InkCacheProbe::Status::TokenPending;out.reason="resource token still loading";return out;}
            const auto resource=u64(token_header.data(),0x28);
            if(resource<0x10000){out.status=InkCacheProbe::Status::TokenPending;out.reason="token loaded without accessible resource handle";return out;}
            // RED4ext.SDK: inkStyleResource : CResource; CResource.path +0x30;
            // DynArray<inkStyle> styles +0x40; style.properties +0x10.
            std::array<std::uint8_t,0x80> resource_header{};
            if(!read_range(read,resource,0,resource_header.data(),resource_header.size()))
                return failure("inkStyleResource header unreadable");
            if(u64(resource_header.data(),0x30)!=key)
                return failure("resource object path does not match requested style");
            out.cached_resource_address=resource; // no ownership; never write via this pointer
            const auto styles=u64(resource_header.data(),0x40);
            const auto style_capacity=u32(resource_header.data(),0x48);
            const auto style_size=u32(resource_header.data(),0x4c);
            if(style_size>512 || style_size>style_capacity || (style_size && styles<0x10000))
                return failure("invalid ink styles array metadata");
            // Keep distinguishing a cached file from a populated Ink style.
            // SDK has themes, overrides, and styleImports arrays at +0x50,
            // +0x60 and +0x70 respectively; counts are at +0x0C.
            const auto themes=u32(resource_header.data(),0x5c);
            const auto overrides=u32(resource_header.data(),0x6c);
            const auto imports=u32(resource_header.data(),0x7c);
            if(themes>1024 || overrides>1024 || imports>1024)
                return failure("invalid auxiliary Ink style array metadata");
            out.styles=style_size;
            out.style_capacity=style_capacity;
            out.themes=themes;
            out.overrides=overrides;
            out.imports=imports;
            if(style_size==0) {
                out.status=InkCacheProbe::Status::EmptyStyles;
                out.reason="cached resource path matched, but Ink styles array is empty";
                return out;
            }
            out.status=InkCacheProbe::Status::StyleAvailable;
            out.reason="Ink styles array populated; inspecting properties";
            // A resource can hold more than one style; search each of the
            // first few (bounded), and bound properties too.
            for(std::uint32_t si=0;si<std::min(style_size,16u);++si) {
                std::array<std::uint8_t,0x20> style{};
                if(!read_range(read,styles,static_cast<std::uint64_t>(si)*0x20,style.data(),style.size()))
                    return failure("inkStyle entry unreadable");
                const auto props=u64(style.data(),0x10);
                const auto prop_capacity=u32(style.data(),0x18);
                const auto prop_count=u32(style.data(),0x1c);
                if(prop_count>4096 || prop_count>prop_capacity || (prop_count && props<0x10000))
                    return failure("invalid ink style properties metadata");
                out.properties+=prop_count;
                for(std::uint32_t pi=0;pi<prop_count;++pi) {
                    std::array<std::uint8_t,0x20> item{};
                    if(!read_range(read,props,static_cast<std::uint64_t>(pi)*0x20,item.data(),item.size()))
                        return failure("inkStyleProperty entry unreadable");
                    if(u64(item.data(),0)!=property_hash)continue;
                    out.found_property=true;
                    const auto tag=u64(item.data(),0x8);
                    // RED4ext.SDK NativeTypes-inl.hpp: Variant stores values
                    // inline only when alignment <= 8. HDRColor is 16-byte
                    // aligned, so it MUST be an externally allocated value.
                    // A tagged inline HDRColor is structurally impossible.
                    if((tag&1)!=0 || tag<0x10000)
                        return failure("HDRColor Variant must use external aligned storage");
                    std::array<std::uint8_t,0x78> rtti{};
                    if(!read_range(read,tag,0,rtti.data(),rtti.size()))
                        return failure("matching Ink Variant RTTI unreadable");
                    if(u64(rtti.data(),0x18)!=cname_hash("HDRColor") ||
                       u32(rtti.data(),0x68)!=0x10 ||
                       u32(rtti.data(),0x74)!=0x10)
                        return failure("matching Ink Variant RTTI is not HDRColor");
                    // The Variant's union contains a POINTER at +0x10, not
                    // the four float values. Honor that engine allocation.
                    const auto storage=u64(item.data(),0x10);
                    if(storage<0x10000 || (storage&0xF)!=0)
                        return failure("HDRColor Variant storage pointer not 16-byte aligned");
                    HdrColor rgba{};
                    if(!read_range(read,storage,0,&rgba,sizeof(rgba)))
                        return failure("HDRColor external storage unreadable");
                    if(!finite_hdr(rgba))
                        return failure("matching HDRColor contains non-finite/invalid floats");
                    const float channels[4]={rgba.red,rgba.green,rgba.blue,rgba.alpha};
                    std::copy(std::begin(channels),std::end(channels),out.rgba);
                    out.hdr_type_match=true;
                    out.status=InkCacheProbe::Status::PropertyCandidate;
                    out.reason="HDRColor RTTI layout matched; read-only color candidate (no owned resource lifetime)";
                    return out;
                }
            }
            return out;
        }
        at=u32(node.data(),0);
    }
    if(at!=UINT32_MAX)return failure("token chain exceeded 128 nodes");
    out.status=InkCacheProbe::Status::NotCached;
    out.reason="style resource not present in ResourceLoader token cache";
    return out;
}
} // namespace dreamui
