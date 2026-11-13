#include "dreamui/ink_cache.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct FakeMemory {
    std::map<std::uint64_t,std::vector<std::uint8_t>> regions;
    void region(std::uint64_t address,std::size_t len) {regions[address]=std::vector<std::uint8_t>(len);}
    void copy(std::uint64_t address,std::size_t off,const void* ptr,std::size_t len){
        auto& v=regions.at(address);
        if(off>v.size() || len>v.size()-off)throw 1;
        std::memcpy(v.data()+off,ptr,len);
    }
    template<class T>void put(std::uint64_t addr,std::size_t off,T value){copy(addr,off,&value,sizeof value);}
    bool read(std::uint64_t address,void* out,std::size_t bytes)const {
        for(const auto& [start,content]:regions) {
            if(address>=start && address-start<=content.size() &&
               bytes<=content.size()-(address-start)) {
                std::memcpy(out,content.data()+address-start,bytes);return true;
            }
        }
        return false;
    }
};
int main() {
    constexpr std::uint64_t loader=0x100000, index=0x200000, nodes=0x300000;
    constexpr std::uint64_t token=0x400000, refs=0x500000, resource=0x600000;
    constexpr std::uint64_t styles=0x700000, props=0x800000, rtti=0x900000, color_storage=0xA00000;
    FakeMemory mem;
    for(auto [start,length]:std::array<std::pair<std::uint64_t,std::size_t>,10>{
        {{loader,0x80},{index,32},{nodes,0x100},{token,0x60},{refs,8},
         {resource,0xc0},{styles,0x20},{props,0x20},{rtti,0x80},{color_storage,0x20}}})
        mem.region(start,length);
    const auto path=std::string("base\\gameplay\\gui\\common\\main_colors.inkstyle");
    const auto key=dreamui::resource_path_hash(path);
    if(!key || key!=dreamui::resource_path_hash("BASE/gameplay/gui//common/main_colors.inkstyle"))
        return 1;
    auto hash=static_cast<std::uint32_t>(key)^static_cast<std::uint32_t>(key>>32);
    for(unsigned i=0;i<8;i++)mem.put(index,i*4,UINT32_MAX);
    mem.put(index,static_cast<std::size_t>(hash%8)*4,0u);
    mem.put(loader,0,index);mem.put(loader,8,1u);mem.put(loader,12,8u);
    mem.put(loader,16,nodes);mem.put(loader,24,8u);mem.put(loader,28,32u);
    mem.put(loader,36,1u);
    mem.put(nodes,0,UINT32_MAX);mem.put(nodes,4,hash);
    mem.put(nodes,8,key);mem.put(nodes,16,token);mem.put(nodes,24,refs);
    mem.put(refs,0,1u);mem.put(refs,4,1u);
    mem.put(token,0x28,resource);mem.put(token,0x48,key);
    mem.put(token,0x58,1u);mem.put(token,0x5c,static_cast<std::uint8_t>(0));
    mem.put(resource,0x30,key);mem.put(resource,0x40,styles);
    mem.put(resource,0x48,1u);mem.put(resource,0x4c,1u);
    mem.put(styles,0x10,props);mem.put(styles,0x18,1u);mem.put(styles,0x1c,1u);
    mem.put(props,0,dreamui::cname_hash("MainColors.Red"));
    mem.put(props,8,static_cast<std::uint64_t>(rtti));
    mem.put(props,16,static_cast<std::uint64_t>(color_storage));
    mem.put(rtti,0x18,dreamui::cname_hash("HDRColor"));
    mem.put(rtti,0x68,static_cast<std::uint32_t>(16));
    mem.put(rtti,0x74,static_cast<std::uint32_t>(16));
    float floats[4]={2.8f,0.9f,1.0f,1.0f};
    mem.copy(color_storage,0,floats,sizeof floats);
    const auto reader=[&](auto a,void* out,auto n){return mem.read(a,out,n);};
    using State=dreamui::InkCacheProbe::Status;
    auto result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::PropertyCandidate || !result.found_token ||
       !result.found_property || result.styles!=1 ||result.properties!=1 ||
       !result.hdr_type_match || result.rgba[0]!=2.8f ||
       result.cached_resource_address!=resource){std::cerr<<"expected property candidate: "<<result.reason<<'\n';return 1;}
    // The same four floats are NOT a color if the Variant RTTI says otherwise.
    mem.put(rtti,0x18,dreamui::cname_hash("Vector4"));
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::Invalid || result.hdr_type_match)return 1;
    mem.put(rtti,0x18,dreamui::cname_hash("HDRColor"));
    mem.put(rtti,0x68,static_cast<std::uint32_t>(8));
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::Invalid)return 1;
    mem.put(rtti,0x68,static_cast<std::uint32_t>(16));
    mem.put(props,8,static_cast<std::uint64_t>(rtti|1)); // inline HDRColor impossible
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::Invalid)return 1;
    mem.put(props,8,static_cast<std::uint64_t>(rtti));
    mem.put(props,16,static_cast<std::uint64_t>(color_storage+8)); // unaligned
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::Invalid)return 1;
    mem.put(props,16,static_cast<std::uint64_t>(color_storage));
    mem.put(resource,0x4c,0u);
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::EmptyStyles || !result.found_token ||
       result.styles!=0 || result.style_capacity!=1)return 1;
    mem.put(resource,0x4c,1u);
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.UnmappedProbe");
    if(result.status!=State::StyleAvailable || result.found_property)return 1;
    mem.put(index,static_cast<std::size_t>(hash%8)*4,UINT32_MAX);
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::NotCached)return 1;
    mem.put(index,static_cast<std::size_t>(hash%8)*4,0u);
    mem.put(refs,0,0u);
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::ExpiredToken)return 1;
    mem.put(refs,0,1u);
    mem.put(token,0x48,key+1);
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::Invalid)return 1;
    mem.put(token,0x48,key);
    mem.put(resource,0x5c,2000u); // reject implausible auxiliary Ink array
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::Invalid)return 1;
    mem.put(resource,0x5c,0u);
    mem.put(loader,12,0u); // invalid map capacity
    result=dreamui::inspect_ink_cache(reader,loader,path,"MainColors.Red");
    if(result.status!=State::Invalid)return 1;
    std::cout<<"Ink cached resource, candidate color, missing and corrupted map tests pass\n";
}
