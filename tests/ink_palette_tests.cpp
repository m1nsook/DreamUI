#include "dreamui/ink_load_patch.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <span>
#include <vector>

using namespace dreamui;
struct Fake {
    std::map<std::uint64_t,std::vector<std::uint8_t>> blocks;
    int writes=0,fail_on=0;
    void region(std::uint64_t a,std::size_t n){blocks[a].resize(n);}
    template<typename T> void put(std::uint64_t a,std::size_t off,const T& v){
        auto& b=blocks.at(a);
        if(off+sizeof(v)>b.size())throw "out of bounds";
        std::memcpy(b.data()+off,&v,sizeof(v));
    }
    bool copy(std::uint64_t a,void* dst,std::size_t n,bool write){
        for(auto& [base,b]:blocks){
            if(a>=base && a-base<=b.size() && n<=b.size()-(a-base)){
                if(write){
                    std::memcpy(b.data()+a-base,dst,n);
                    ++writes;
                    if(writes==fail_on)return false; // partial native write
                }else std::memcpy(dst,b.data()+a-base,n);
                return true;
            }
        }
        return false;
    }
};
#define CHECK(x) do{if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
int main(){
    Fake mem;
    constexpr std::uint64_t object=0x100000,styles=0x200000;
    constexpr std::uint64_t props1=0x300000,props2=0x310000;
    constexpr std::uint64_t rtti=0x400000;
    constexpr std::uint64_t clr1=0x500000,clr2=0x510000,clr3=0x520000;
    const auto path=resource_path_hash(R"(base\gameplay\gui\common\main_colors.inkstyle)");
    for(auto [ptr,len]:std::array<std::pair<std::uint64_t,std::size_t>,7>{{
        {object,0xC0},{styles,0x40},{props1,0x40},{props2,0x20},
        {rtti,0x80},{clr1,0x20},{clr2,0x20}}})mem.region(ptr,len);
    mem.region(clr3,0x20);
    mem.put(object,0x28,std::uint64_t(0xAA55));
    mem.put(object,0x30,path);
    mem.put(object,0x40,styles);
    mem.put(object,0x48,2u);
    mem.put(object,0x4c,2u);
    mem.put(styles,0,cname_hash("MainColors"));
    mem.put(styles,8,cname_hash("Default"));
    mem.put(styles,0x20,cname_hash("OtherColors"));
    mem.put(styles,0x28,cname_hash("Active"));
    mem.put(styles,0x10,props1);
    mem.put(styles,0x18,2u);
    mem.put(styles,0x1c,2u);
    mem.put(styles,0x20+0x10,props2);
    mem.put(styles,0x20+0x18,1u);
    mem.put(styles,0x20+0x1c,1u);
    mem.put(rtti,0x18,cname_hash("HDRColor"));
    mem.put(rtti,0x68,16u);
    mem.put(rtti,0x74,16u);
    mem.put(props1,0,cname_hash("MainColors.Red"));
    mem.put(props1,8,rtti);
    mem.put(props1,16,clr1);
    mem.put(props1,0x20+0,cname_hash("MainColors.PanelBlue"));
    mem.put(props1,0x20+8,rtti);
    mem.put(props1,0x20+16,clr2);
    mem.put(props2,0,cname_hash("MainColors.ActiveYellow"));
    mem.put(props2,8,rtti);
    mem.put(props2,16,clr3);
    const HdrColor red{1.1761f,0.38f,0.3476f,1.0f};
    const HdrColor panel{0.35f,0.9f,1.2f,0.8f};
    const HdrColor yellow{1.3f,1.05f,0.12f,0.75f};
    mem.put(clr1,0,red);
    mem.put(clr2,0,panel);
    mem.put(clr3,0,yellow);
    auto rd=[&](std::uint64_t a,void* dst,std::size_t n){return mem.copy(a,dst,n,false);};
    auto wr=[&](std::uint64_t a,const void* src,std::size_t n){
        return mem.copy(a,const_cast<void*>(src),n,true);
    };
    std::array<InkPaletteEntry,3> palette{{
        {"MainColors.Red",Rgb{234,240,242}},
        {"MainColors.PanelBlue",Rgb{120,175,200}},
        {"MainColors.ActiveYellow",Rgb{246,199,70}}
    }};
    // A complete read-only inspection must agree with the patcher's
    // validation, without ever calling the memory writer.
    InkOwnedColorPreimage owned{};
    auto inspected=inspect_loaded_ink_palette(rd,object,path,palette,&owned);
    CHECK(inspected.eligible && inspected.matched==3 && inspected.styles==2);
    CHECK(owned.valid() && owned.entry_count==3 && owned.style_count==2);
    CHECK(owned.resource_path_hash==path && owned.resource_instance_id==0xAA55);
    CHECK(owned.entries[0].style_id==cname_hash("MainColors"));
    CHECK(owned.entries[0].style_state==cname_hash("Default"));
    CHECK(owned.entries[0].property_hash==cname_hash("MainColors.Red"));
    CHECK(owned.entries[0].original==red);
    CHECK(owned.entries[1].original==panel && owned.entries[2].original==yellow);
    CHECK(owned.entries[2].style_id==cname_hash("OtherColors"));
    CHECK(owned.entries[2].style_state==cname_hash("Active"));
    CHECK(owned.entries[0].proposed!=owned.entries[0].original);
    CHECK(mem.writes==0);
    // Keep an independent value copy. It must survive native styles being
    // consumed, even though it cannot reconstruct the full inkStyle array.
    const auto retained=owned;
    mem.put(object,0x28,std::uint64_t(0));
    inspected=inspect_loaded_ink_palette(rd,object,path,palette,&owned);
    CHECK(!inspected.eligible && inspected.failure==InkLoadPatchStatus::InvalidResource);
    CHECK(!owned.valid() && retained.valid());
    mem.put(object,0x28,std::uint64_t(0xAA55));
    mem.put(object,0x4c,0u);
    inspected=inspect_loaded_ink_palette(rd,object,path,palette,&owned);
    CHECK(!inspected.eligible && inspected.failure==InkLoadPatchStatus::NoStyles);
    CHECK(!owned.valid() && retained.entries[0].original==red);
    mem.put(object,0x4c,2u);
    CHECK(mem.writes==0);

    InkLoadJournal journal{};
    auto result=patch_loaded_ink_palette(rd,wr,object,path,palette,&journal);
    CHECK(result.status==InkLoadPatchStatus::Applied);
    CHECK(result.written==3 && result.matched==3 && journal.count==3);
    HdrColor a{},b{},c{};
    CHECK(rd(clr1,&a,sizeof(a)) && rd(clr2,&b,sizeof(b)) &&
          rd(clr3,&c,sizeof(c)));
    CHECK(a.alpha==red.alpha && b.alpha==panel.alpha &&
          c.alpha==yellow.alpha);
    CHECK(a.red>red.red*0.9f && b.blue>0.9f && c.green>0.9f);
    CHECK(restore_loaded_ink_color(rd,wr,object,journal)==InkLoadRestoreStatus::Restored);
    CHECK(rd(clr1,&a,sizeof(a)) && a==red);
    CHECK(rd(clr2,&b,sizeof(b)) && b==panel);
    CHECK(rd(clr3,&c,sizeof(c)) && c==yellow);
    journal.clear();

    const auto writes=mem.writes;
    std::array<InkPaletteEntry,4> missing{{
        palette[0],palette[1],palette[2],{"MainColors.NotInGame",Rgb{1,2,3}}
    }};
    inspected=inspect_loaded_ink_palette(rd,object,path,missing);
    CHECK(!inspected.eligible &&
          inspected.failure==InkLoadPatchStatus::NoMatchingProperty);
    CHECK(mem.writes==writes);
    result=patch_loaded_ink_palette(rd,wr,object,path,missing,&journal);
    CHECK(result.status==InkLoadPatchStatus::NoMatchingProperty);
    CHECK(mem.writes==writes && !journal.active());
    std::array<InkPaletteEntry,2> duplicate{{palette[0],palette[0]}};
    CHECK(patch_loaded_ink_palette(rd,wr,object,path,duplicate).status==
          InkLoadPatchStatus::InvalidResource);
    CHECK(mem.writes==writes);

    // If any requested property has an incorrect Variant type, none change.
    mem.put(props2,8,rtti|1);
    inspected=inspect_loaded_ink_palette(rd,object,path,palette);
    CHECK(!inspected.eligible && inspected.failure==InkLoadPatchStatus::InvalidProperty);
    CHECK(mem.writes==writes);
    CHECK(patch_loaded_ink_palette(rd,wr,object,path,palette).status==
          InkLoadPatchStatus::InvalidProperty);
    CHECK(mem.writes==writes);
    mem.put(props2,8,rtti);

    // Third native write partially succeeds, so all three originals must
    // be restored before reporting WriteFailed.
    mem.fail_on=mem.writes+3;
    result=patch_loaded_ink_palette(rd,wr,object,path,palette,&journal);
    CHECK(result.status==InkLoadPatchStatus::WriteFailed);
    CHECK(!journal.active());
    CHECK(rd(clr1,&a,sizeof(a)) && a==red);
    CHECK(rd(clr2,&b,sizeof(b)) && b==panel);
    CHECK(rd(clr3,&c,sizeof(c)) && c==yellow);
    mem.fail_on=0;
    // No retained pointer, array header, or RTTI address is needed after
    // dropping all fake engine memory. Snapshot values stay usable.
    mem.blocks.clear();
    CHECK(retained.valid() && retained.entries[0].original==red);
    CHECK(retained.entries[1].original==panel &&
          retained.entries[2].original==yellow);
    CHECK(retained.entries[0].style_index==0 &&
          retained.entries[2].style_index==1);
    std::cout<<"Native palette preimage is owned, pointer-free, and transaction-safe\n";
}