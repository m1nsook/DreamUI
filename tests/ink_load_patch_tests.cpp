#include "dreamui/ink_load_patch.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace dreamui;
struct Memory {
    std::map<std::uint64_t,std::vector<std::uint8_t>> entries;
    int failing_write=0;
    void region(std::uint64_t addr,std::size_t len){entries[addr].resize(len);}
    template<class T> void put(std::uint64_t addr,std::size_t off,T value) {
        auto& v=entries.at(addr);
        if(off+sizeof value>v.size())throw "test out of bounds";
        std::memcpy(v.data()+off,&value,sizeof(value));
    }
    bool transfer(std::uint64_t addr,void* ptr,std::size_t size,bool writer) {
        for(auto& [base,v]:entries) {
            if(addr>=base && addr-base<=v.size() && size<=v.size()-(addr-base)) {
                if(writer) {
                    std::memcpy(v.data()+addr-base,ptr,size);
                    if(failing_write>0){--failing_write;return false;}
                }else std::memcpy(ptr,v.data()+addr-base,size);
                return true;
            }
        }
        return false;
    }
};
#define CHECK(expr) do{ if(!(expr)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#expr<<"\n";return 1;} }while(0)
int main(){
    constexpr std::uint64_t resource=0x100000,styles=0x200000,properties=0x300000,
        meta=0x400000,storage=0x500000;
    const auto path=resource_path_hash(R"(base\gameplay\gui\common\themes\main_colors_default.inkstyle)");
    Memory mem;
    for(const auto& [addr,len]:std::array<std::pair<std::uint64_t,std::size_t>,5>{{
        {resource,0xC0},{styles,0x20},{properties,0x20},{meta,0x80},{storage,0x20}}})
        mem.region(addr,len);
    mem.put(resource,0x30,path);
    mem.put(resource,0x40,styles);
    mem.put(resource,0x48,1u);
    mem.put(resource,0x4c,1u);
    mem.put(styles,0x10,properties);
    mem.put(styles,0x18,1u);
    mem.put(styles,0x1c,1u);
    mem.put(properties,0,cname_hash("MainColors.Red"));
    mem.put(properties,8,meta);
    mem.put(properties,16,storage);
    mem.put(meta,0x18,cname_hash("HDRColor"));
    mem.put(meta,0x68,16u);
    mem.put(meta,0x74,16u);
    const HdrColor vanilla{4.0f,0.5f,0.1f,0.625f};
    mem.put(storage,0,vanilla);
    auto read=[&](std::uint64_t address,void* ptr,std::size_t n) {
        return mem.transfer(address,ptr,n,false);
    };
    auto write=[&](std::uint64_t address,const void* ptr,std::size_t n) {
        return mem.transfer(address,const_cast<void*>(ptr),n,true);
    };
    const Rgb selected{234,240,242};
    auto result=patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected);
    CHECK(result.status==InkLoadPatchStatus::Applied && result.written==1 && result.matched==1);
    CHECK(result.original==vanilla);
    HdrColor after{};
    CHECK(read(storage,&after,sizeof(after)));
    CHECK(after.red>3.6f && after.red<3.7f && after.blue>3.7f && after.alpha==vanilla.alpha);
    CHECK(result.preview==after);
    CHECK(patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected).status==InkLoadPatchStatus::Applied);
    mem.put(storage,0,vanilla);

    CHECK(patch_loaded_ink_color(read,write,resource,path+1,"MainColors.Red",selected).status==InkLoadPatchStatus::InvalidResource);
    CHECK(patch_loaded_ink_color(read,write,resource,path,"NotTheProperty",selected).status==InkLoadPatchStatus::NoMatchingProperty);
    mem.put(properties,8,meta|1); // HDR cannot be inline.
    CHECK(patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected).status==InkLoadPatchStatus::InvalidProperty);
    mem.put(properties,8,meta);
    mem.put(meta,0x18,cname_hash("Vector4"));
    CHECK(patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected).status==InkLoadPatchStatus::InvalidProperty);
    mem.put(meta,0x18,cname_hash("HDRColor"));
    mem.put(storage,0,HdrColor{0,0,0,1});
    CHECK(patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected).status==InkLoadPatchStatus::InvalidProperty);
    mem.put(storage,0,vanilla);

    mem.failing_write=1;
    result=patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected);
    CHECK(result.status==InkLoadPatchStatus::WriteFailed);
    CHECK(read(storage,&after,sizeof(after)) && after==vanilla);

    // Two failing calls: one partial write, then rollback itself signals
    // failure even after writing; fail closed and expose rollback failure.
    mem.failing_write=2;
    result=patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected);
    CHECK(result.status==InkLoadPatchStatus::RollbackFailed);

    mem.put(resource,0x4c,0u);
    CHECK(patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected).status==InkLoadPatchStatus::NoStyles);
    mem.put(resource,0x4c,999u);
    CHECK(patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected).status==InkLoadPatchStatus::InvalidResource);
    std::cout<<"Native Ink load-stage color transactions and failure rollback tests passed\n";
}
