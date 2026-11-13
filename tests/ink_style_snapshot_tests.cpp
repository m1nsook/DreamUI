#include "dreamui/ink_style_snapshot.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

using namespace dreamui;
namespace {
struct Fake {
    std::map<std::uint64_t,std::vector<std::uint8_t>> regions;
    void region(std::uint64_t address,std::size_t size) {
        regions[address].resize(size);
    }
    template<typename T>
    void put(std::uint64_t address,std::size_t off,const T& item) {
        auto& bytes=regions.at(address);
        if(off+sizeof(T)>bytes.size())throw "fixture out of range";
        std::memcpy(bytes.data()+off,&item,sizeof(item));
    }
    bool read(std::uint64_t address,void* out,std::size_t size) const {
        for(const auto& [base,bytes]:regions) {
            if(address>=base && address-base<=bytes.size() &&
               size<=bytes.size()-(address-base)) {
                std::memcpy(out,bytes.data()+(address-base),size);
                return true;
            }
        }
        return false;
    }
};
#define CHECK(x) do { if(!(x)) { std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1; } } while(false)
}

int main() {
    Fake mem;
    constexpr std::uint64_t resource=0x100000;
    constexpr std::uint64_t styles=0x200000;
    constexpr std::uint64_t p1=0x300000,p2=0x310000;
    constexpr std::uint64_t rtti=0x400000,other_rtti=0x410000;
    constexpr std::uint64_t c1=0x500000,c2=0x510000,c3=0x520000;
    constexpr auto path=resource_path_hash(R"(base\gameplay\gui\common\main_colors.inkstyle)");
    for(auto [addr,bytes]:std::array<std::pair<std::uint64_t,std::size_t>,9>{{
        {resource,0xC0},{styles,0x40},{p1,0x40},{p2,0x20},
        {rtti,0x80},{other_rtti,0x80},{c1,0x20},{c2,0x20},{c3,0x20}
    }})mem.region(addr,bytes);
    mem.put(resource,0x28,std::uint64_t(0xAA55));
    mem.put(resource,0x30,path);
    mem.put(resource,0x40,styles);
    mem.put(resource,0x48,2u);
    mem.put(resource,0x4C,2u);
    mem.put(styles,0,cname_hash("MainColors"));
    mem.put(styles,8,cname_hash("Default"));
    mem.put(styles,0x10,p1);
    mem.put(styles,0x18,2u);
    mem.put(styles,0x1C,2u);
    mem.put(styles,0x20,cname_hash("Accent"));
    mem.put(styles,0x28,cname_hash("Active"));
    mem.put(styles,0x30,p2);
    mem.put(styles,0x38,1u);
    mem.put(styles,0x3C,1u);
    mem.put(rtti,0x18,cname_hash("HDRColor"));
    mem.put(rtti,0x68,16u);
    mem.put(rtti,0x74,16u);
    mem.put(other_rtti,0x18,cname_hash("Float"));
    mem.put(other_rtti,0x68,4u);
    mem.put(other_rtti,0x74,4u);
    mem.put(p1,0,cname_hash("MainColors.Red"));
    mem.put(p1,8,rtti);
    mem.put(p1,16,c1);
    mem.put(p1,0x20,cname_hash("MainColors.PanelBlue"));
    mem.put(p1,0x28,rtti);
    mem.put(p1,0x30,c2);
    mem.put(p2,0,cname_hash("MainColors.ActiveYellow"));
    mem.put(p2,8,rtti);
    mem.put(p2,16,c3);
    const HdrColor red{1.1761f,0.38f,0.3476f,1.0f};
    const HdrColor blue{0.35f,0.9f,1.2f,0.8f};
    const HdrColor yellow{1.3f,1.05f,0.12f,0.75f};
    mem.put(c1,0,red);
    mem.put(c2,0,blue);
    mem.put(c3,0,yellow);
    const MemoryRead rd=[&](std::uint64_t a,void* p,std::size_t n) {
        return mem.read(a,p,n);
    };
    InkOwnedStyleTree tree;
    auto result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::Captured);
    CHECK(result.observed_styles==2 && result.observed_properties==3);
    CHECK(tree.complete() && tree.styles.size()==2 && tree.properties.size()==3);
    CHECK(tree.resource_path_hash==path && tree.resource_instance_id==0xAA55);
    CHECK(tree.styles[0].style_id==cname_hash("MainColors"));
    CHECK(tree.styles[0].state==cname_hash("Default"));
    CHECK(tree.styles[0].first_property==0 && tree.styles[0].property_count==2);
    CHECK(tree.styles[1].style_id==cname_hash("Accent"));
    CHECK(tree.styles[1].state==cname_hash("Active"));
    CHECK(tree.styles[1].first_property==2 && tree.styles[1].property_count==1);
    CHECK(tree.properties[0].property_path_hash==cname_hash("MainColors.Red"));
    CHECK(tree.properties[0].color==red && tree.properties[1].color==blue &&
          tree.properties[2].color==yellow);
    const auto retained=tree;

    // A bad path and missing instance ID never yield a partially-owned tree.
    result=capture_loaded_ink_style_tree(rd,resource,path+1,tree);
    CHECK(result.status==InkStyleCaptureStatus::InvalidResource && !tree.complete());
    mem.put(resource,0x28,std::uint64_t(0));
    result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::InvalidResource && !tree.complete());
    mem.put(resource,0x28,std::uint64_t(0xAA55));

    // NoStyles is not a successful reconstruction recipe.
    mem.put(resource,0x4C,0u);
    result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::NoStyles && !tree.complete());
    mem.put(resource,0x4C,2u);

    // Any other Variant type must fail closed: arbitrary native types
    // can allocate resources which cannot safely be byte-copied.
    mem.put(p2,8,other_rtti);
    result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::UnsupportedVariant);
    CHECK(result.unsupported_properties==1 && !tree.complete());
    mem.put(p2,8,rtti);

    // An inline HDRColor and an incorrectly aligned pointer are invalid.
    mem.put(p2,8,rtti|1u);
    result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::InvalidVariant && !tree.complete());
    mem.put(p2,8,rtti);
    mem.put(p2,16,c3+8u);
    result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::InvalidVariant && !tree.complete());
    mem.put(p2,16,c3);

    // Bounds and bad values must not leak part of an accepted style tree.
    mem.put(styles,0x1C,1025u);
    mem.put(styles,0x18,1025u);
    result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::LimitExceeded && !tree.complete());
    mem.put(styles,0x1C,2u);
    mem.put(styles,0x18,2u);
    const HdrColor invalid{std::numeric_limits<float>::quiet_NaN(),0,0,1};
    mem.put(c2,0,invalid);
    result=capture_loaded_ink_style_tree(rd,resource,path,tree);
    CHECK(result.status==InkStyleCaptureStatus::InvalidVariant && !tree.complete());
    mem.put(c2,0,blue);

    // Retained values outlive all fake engine objects, without pointers.
    mem.regions.clear();
    CHECK(retained.complete());
    CHECK(retained.styles.size()==2 && retained.properties.size()==3);
    CHECK(retained.properties[0].color==red);
    CHECK(retained.properties[2].color==yellow);
    std::cout<<"Complete typed Ink style tree capture is owned and fail-closed\n";
}
