#include "dreamui/ink_graph.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace dreamui;

struct Memory {
    std::map<std::uint64_t,std::vector<std::uint8_t>> regions;
    void region(std::uint64_t base,std::size_t len) {regions[base].resize(len);}
    template<class T>
    void put(std::uint64_t base,std::size_t off,T value) {
        auto& bytes=regions.at(base);
        if(off+sizeof value>bytes.size())throw "mock out-of-bounds";
        std::memcpy(bytes.data()+off,&value,sizeof value);
    }
    void copy(std::uint64_t base,std::size_t off,const void* src,std::size_t n) {
        auto& b=regions.at(base);
        if(off+n>b.size())throw "mock copy out-of-bounds";
        std::memcpy(b.data()+off,src,n);
    }
    bool read(std::uint64_t addr,void* dest,std::size_t len) const {
        for(const auto& [base,bytes]:regions) {
            if(addr>=base && addr-base<=bytes.size() &&
               len<=bytes.size()-(addr-base)) {
                std::memcpy(dest,bytes.data()+addr-base,len);return true;
            }
        }
        return false;
    }
};
#define CHECK(expr) do {if(!(expr)){ \
    std::cerr<<"FAIL line "<<__LINE__<<" : "<<#expr<<"\n";return 1; \
}}while(false)

int main(){
    constexpr std::uint64_t root=0x100000,theme_array=0x110000;
    constexpr std::uint64_t token=0x120000,refcount=0x130000;
    constexpr std::uint64_t child=0x200000,styles=0x210000,props=0x220000;
    constexpr std::uint64_t rtti=0x230000,hdr_storage=0x240000;
    const auto root_hash=resource_path_hash(R"(base\gameplay\gui\common\main_colors.inkstyle)");
    const auto child_hash=resource_path_hash(R"(base\gameplay\gui\common\themes\main_colors_default.inkstyle)");
    Memory mem;
    for(auto [base,length]:std::array<std::pair<std::uint64_t,std::size_t>,9>{{
        {root,0xC0},{theme_array,0x40},{token,0x60},{refcount,8},
        {child,0xC0},{styles,0x20},{props,0x20},{rtti,0x80},{hdr_storage,0x10}}})
        mem.region(base,length);
    mem.put(root,0x30,root_hash);
    mem.put(root,0x50,theme_array);
    mem.put(root,0x58,1u);
    mem.put(root,0x5c,1u);
    mem.put(theme_array,0,child_hash);
    mem.put(theme_array,8,token);
    mem.put(theme_array,16,refcount);
    mem.put(refcount,0,1u);
    mem.put(token,0x28,child);
    mem.put(token,0x48,child_hash);
    mem.put(token,0x58,1u);
    mem.put(child,0x30,child_hash);
    mem.put(child,0x40,styles);
    mem.put(child,0x48,1u);
    mem.put(child,0x4c,1u);
    mem.put(styles,0x10,props);
    mem.put(styles,0x18,1u);
    mem.put(styles,0x1c,1u);
    mem.put(props,0,cname_hash("MainColors.Red"));
    mem.put(props,8,rtti); // IMPORTANT: HDRColor Variant is NOT inline.
    mem.put(props,16,hdr_storage);
    mem.put(rtti,0x18,cname_hash("HDRColor"));
    mem.put(rtti,0x68,16u);
    mem.put(rtti,0x74,16u);
    const HdrColor color{3.45f,1.2f,0.11f,0.625f};
    mem.copy(hdr_storage,0,&color,sizeof color);
    const MemoryRead read=[&](auto address,void* dst,auto size){
        return mem.read(address,dst,size);
    };
    auto result=inspect_ink_style_graph(read,root,root_hash,"MainColors.Red");
    CHECK(result.valid && result.found_color);
    CHECK(result.resources_visited==2 && result.references_loaded==1);
    CHECK(result.styles_visited==1 && result.properties_visited==1);
    CHECK(result.owner_path_hash==child_hash && result.color==color);
    CHECK(result.color.red>1.f && result.color.alpha==0.625f);

    // A nonexistent property is not an engine resource error.
    result=inspect_ink_style_graph(read,root,root_hash,"MainColors.NotFound");
    CHECK(result.valid && !result.found_color && result.resources_visited==2);

    // Child strong reference dropped: never dereference a stale token.
    mem.put(refcount,0,0u);
    result=inspect_ink_style_graph(read,root,root_hash,"MainColors.Red");
    CHECK(result.valid && !result.found_color && result.resources_visited==1);
    mem.put(refcount,0,1u);

    // A completed token must match its declared Ref<ResourcePath>.
    mem.put(token,0x48,child_hash+1);
    result=inspect_ink_style_graph(read,root,root_hash,"MainColors.Red");
    CHECK(result.valid && !result.found_color && result.references_loaded==0);
    mem.put(token,0x48,child_hash);

    // Mismatched child CResource.path is a hard failure, not an edit target.
    mem.put(child,0x30,root_hash);
    result=inspect_ink_style_graph(read,root,root_hash,"MainColors.Red");
    CHECK(!result.valid && !result.found_color);
    mem.put(child,0x30,child_hash);

    // Even if the underlying floats look right, wrong RTTI is rejected.
    mem.put(rtti,0x18,cname_hash("Vector4"));
    result=inspect_ink_style_graph(read,root,root_hash,"MainColors.Red");
    CHECK(!result.valid && !result.found_color);
    mem.put(rtti,0x18,cname_hash("HDRColor"));

    // Native HDRColor outside the Variant inline buffer.
    mem.put(props,8,rtti|1);
    result=inspect_ink_style_graph(read,root,root_hash,"MainColors.Red");
    CHECK(!result.valid && !result.found_color);
    mem.put(props,8,rtti);

    // Root count corruption.
    mem.put(root,0x5c,5000u);
    result=inspect_ink_style_graph(read,root,root_hash,"MainColors.Red");
    CHECK(!result.valid);
    mem.put(root,0x5c,1u);
    CHECK(inspect_ink_style_graph(read,root,root_hash,"MainColors.Red").found_color);
    std::cout<<"Linked Ink style themes, external HDRColor and safety guards passed\n";
}
