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
    int fail_write_number=0;
    int writes_seen=0;
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
                    ++writes_seen;
                    if(fail_write_number>0 && writes_seen==fail_write_number)
                        return false;
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
        meta=0x400000,storage=0x500000,properties2=0x310000,storage2=0x510000;
    const auto path=resource_path_hash(R"(base\gameplay\gui\common\themes\main_colors_default.inkstyle)");
    Memory mem;
    for(const auto& [addr,len]:std::array<std::pair<std::uint64_t,std::size_t>,5>{{
        {resource,0xC0},{styles,0x40},{properties,0x20},{meta,0x80},{storage,0x20}}})
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

    // Two matching properties in different inkStyles: the second write
    // partially fails after the first succeeded. Both MUST be restored.
    mem.region(properties2,0x20);
    mem.region(storage2,0x20);
    mem.put(styles,0x20+0x10,properties2);
    mem.put(styles,0x20+0x18,1u);
    mem.put(styles,0x20+0x1c,1u);
    mem.put(properties2,0,cname_hash("MainColors.Red"));
    mem.put(properties2,8,meta);
    mem.put(properties2,16,storage2);
    const HdrColor second_vanilla{1.4f,0.2f,0.3f,1.0f};
    mem.put(storage2,0,second_vanilla);
    mem.put(storage,0,vanilla);
    mem.put(resource,0x48,2u);
    mem.put(resource,0x4c,2u);

    mem.writes_seen=0;
    mem.fail_write_number=2;
    result=patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected);
    CHECK(result.status==InkLoadPatchStatus::WriteFailed);
    CHECK(read(storage,&after,sizeof(after)) && after==vanilla);
    HdrColor second_after{};
    CHECK(read(storage2,&second_after,sizeof(second_after)) &&
          second_after==second_vanilla);
    mem.fail_write_number=0;

    // A malformed *later* property must cause no writes to an earlier
    // perfectly valid one: validate the complete journal before mutation.
    const auto writes_before=mem.writes_seen;
    mem.put(properties2,8,meta|1);
    result=patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected);
    CHECK(result.status==InkLoadPatchStatus::InvalidProperty);
    CHECK(mem.writes_seen==writes_before);
    CHECK(read(storage,&after,sizeof(after)) && after==vanilla);
    mem.put(properties2,8,meta);
    result=patch_loaded_ink_color(read,write,resource,path,"MainColors.Red",selected);
    CHECK(result.status==InkLoadPatchStatus::Applied && result.matched==2 &&
          result.written==2);

    // Record every successful color write so the same authenticated game
    // resource can later restore ALL original HDRColor values exactly.
    mem.put(resource,0x28,std::uint64_t(0xA1B2C3D4));
    mem.put(storage,0,vanilla);
    mem.put(storage2,0,second_vanilla);
    InkLoadJournal journal{};
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::NoJournal);
    result=patch_loaded_ink_color(read,write,resource,path,
                                  "MainColors.Red",selected,&journal);
    CHECK(result.status==InkLoadPatchStatus::Applied && journal.active() &&
          journal.count==2);
    CHECK(journal.entries[0].before==vanilla &&
          journal.entries[1].before==second_vanilla);
    const auto first_applied=journal.entries[0].applied;
    const auto second_applied=journal.entries[1].applied;
    CHECK(restore_loaded_ink_color(read,write,resource+1,journal)==
          InkLoadRestoreStatus::StaleResource);
    CHECK(patch_loaded_ink_color(read,write,resource,path,
          "MainColors.Red",selected,&journal).status==
          InkLoadPatchStatus::JournalOccupied);

    // Object identity changes even at the same address: never write.
    mem.put(resource,0x28,std::uint64_t(0xA1B2C3D5));
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::StaleResource);
    mem.put(resource,0x28,std::uint64_t(0xA1B2C3D4));

    // If another mod modifies a property after DreamUI, don't overwrite it.
    mem.put(storage2,0,HdrColor{4.f,3.f,2.f,1.f});
    const auto before_reject=mem.writes_seen;
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::ChangedExternally);
    CHECK(mem.writes_seen==before_reject);
    mem.put(storage2,0,second_applied);

    // Property names must remain identical even if the Variant stays valid.
    mem.put(properties2,0,cname_hash("MainColors.Blue"));
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::ChangedExternally);
    mem.put(properties2,0,cname_hash("MainColors.Red"));

    // If the native Variant allocation changes, don't write to a stale ptr.
    mem.put(properties2,16,std::uint64_t(storage));
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::ChangedExternally);
    mem.put(properties2,16,std::uint64_t(storage2));

    // Native restore can partially fail. The rollback returns to the
    // applied state, leaving a retryable journal with no mixed colors.
    mem.fail_write_number=mem.writes_seen+2;
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::WriteFailed);
    CHECK(journal.active());
    CHECK(read(storage,&after,sizeof(after)) && after==first_applied);
    CHECK(read(storage2,&second_after,sizeof(second_after)) &&
          second_after==second_applied);
    mem.fail_write_number=0;
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::Restored);
    CHECK(read(storage,&after,sizeof(after)) && after==vanilla);
    CHECK(read(storage2,&second_after,sizeof(second_after)) &&
          second_after==second_vanilla);
    // The owner of the resource lifetime, not the memory writer, clears
    // a completed journal. A stale raw snapshot is never an owning handle.
    journal.clear();
    CHECK(!journal.active());
    CHECK(restore_loaded_ink_color(read,write,resource,journal)==
          InkLoadRestoreStatus::NoJournal);
    std::cout<<"Native Ink load-stage color transactions and failure rollback tests passed\n";
}
