#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/win_clone_probe.hpp"
#include "dreamui/ink_cache.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <new>

using namespace dreamui;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)

namespace {
constexpr std::uint64_t kPath=0x1234567890ABCDEFULL;
constexpr std::uint64_t kSourceId=0x11223344;
std::array<std::uint8_t,0x20> g_type{};
std::array<std::uint8_t,0x20> g_wrong_type{};
std::array<std::uint64_t,27> g_object_vtable{};
std::array<std::uint64_t,5> g_allocator_vtable{};
struct FakeAllocator { std::uint64_t* vtable; };
FakeAllocator g_allocator{g_allocator_vtable.data()};
void* g_source=nullptr;
std::uint64_t g_source_styles=0;
int g_clone_calls=0,g_destruct_calls=0,g_free_calls=0,g_weak_calls=0;
bool g_wrong_clone_type=false,g_alias_clone=false,g_share_styles=false;
std::uint64_t get64(void* p,std::size_t off) {
    std::uint64_t x=0;std::memcpy(&x,static_cast<char*>(p)+off,sizeof(x));return x;
}
void put64(void* p,std::size_t off,std::uint64_t x) {
    std::memcpy(static_cast<char*>(p)+off,&x,sizeof(x));
}
void put32(void* p,std::size_t off,std::uint32_t x) {
    std::memcpy(static_cast<char*>(p)+off,&x,sizeof(x));
}
void* new_resource(std::uint64_t id,std::uint64_t styles_ptr) {
    auto* o=::operator new(0xC0);
    std::memset(o,0,0xC0);
    put64(o,0,reinterpret_cast<std::uint64_t>(g_object_vtable.data()));
    put64(o,0x28,id);put64(o,0x30,kPath);
    put64(o,0x40,styles_ptr);
    put32(o,0x48,2);put32(o,0x4c,2);
    return o;
}
std::uint64_t new_styles() {
    auto* p=::operator new(0x40);
    std::memset(p,0,0x40);
    return reinterpret_cast<std::uint64_t>(p);
}
void* __fastcall get_type(void* object) {
    if(g_wrong_clone_type && object!=g_source)
        return g_wrong_type.data();
    return g_type.data();
}
void* __fastcall get_allocator(void*) {return &g_allocator;}
void __fastcall fake_destroy(void* object,unsigned flags) {
    if(flags!=0)std::abort();
    ++g_destruct_calls;
    const auto styles=get64(object,0x40);
    if(styles && styles!=g_source_styles)
        ::operator delete(reinterpret_cast<void*>(styles));
}
bool __fastcall can_destroy(void*) {return true;}
void __fastcall allocator_free(void*,void* allocation_ptr) {
    struct AllocationResult {void* instance;std::size_t size;};
    auto* allocation=static_cast<AllocationResult*>(allocation_ptr);
    ++g_free_calls;
    ::operator delete(allocation->instance);
}
void __fastcall dec_weak_ref(InkOwnedNativeHandle* output) {
    ++g_weak_calls;
    auto* ref=reinterpret_cast<std::uint32_t*>(output->ref_count);
    if(ref[0]!=0 || ref[1]!=1)std::abort();
    delete[] ref;
}
void* __fastcall clone_fn(InkOwnedNativeHandle* output,const InkBorrowedHandle* input) {
    ++g_clone_calls;
    if(g_alias_clone) {
        *output={input->instance,input->ref_count};
        return output;
    }
    const auto styles=g_share_styles ? g_source_styles : new_styles();
    auto* clone=new_resource(0xBB55,styles);
    auto* refs=new std::uint32_t[2]{1,1};
    *output={reinterpret_cast<std::uint64_t>(clone),
             reinterpret_cast<std::uint64_t>(refs)};
    return output;
}
WinInkCloneProbeConfig config() {
    const std::array<std::uintptr_t,7> addrs{{
        reinterpret_cast<std::uintptr_t>(&clone_fn),
        reinterpret_cast<std::uintptr_t>(&dec_weak_ref),
        reinterpret_cast<std::uintptr_t>(&get_type),
        reinterpret_cast<std::uintptr_t>(&get_allocator),
        reinterpret_cast<std::uintptr_t>(&fake_destroy),
        reinterpret_cast<std::uintptr_t>(&can_destroy),
        reinterpret_cast<std::uintptr_t>(&allocator_free)
    }};
    const auto bounds=std::minmax_element(addrs.begin(),addrs.end());
    const auto begin=*bounds.first & ~std::uintptr_t{0xfff};
    const auto end=(*bounds.second+0x2000) & ~std::uintptr_t{0xfff};
    return {addrs[0],addrs[1],begin,end};
}
}
int main() {
    std::uint64_t hash=cname_hash("inkStyleResource");
    std::memcpy(g_type.data()+0x18,&hash,8);
    hash=cname_hash("NotInk");
    std::memcpy(g_wrong_type.data()+0x18,&hash,8);
    g_object_vtable[0]=reinterpret_cast<std::uint64_t>(&get_type);
    g_object_vtable[2]=reinterpret_cast<std::uint64_t>(&get_allocator);
    g_object_vtable[3]=reinterpret_cast<std::uint64_t>(&fake_destroy);
    g_object_vtable[26]=reinterpret_cast<std::uint64_t>(&can_destroy);
    g_allocator_vtable[4]=reinterpret_cast<std::uint64_t>(&allocator_free);
    g_source_styles=new_styles();
    g_source=new_resource(kSourceId,g_source_styles);
    auto* refs=new std::uint32_t[2]{1,1};
    const InkBorrowedHandle source{
        reinterpret_cast<std::uint64_t>(g_source),
        reinterpret_cast<std::uint64_t>(refs)};
    const InkCloneIdentity identity{kPath,kSourceId};

    auto cfg=config();
    auto result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::Released);
    CHECK(result.source_styles==2 && result.cloned_styles==2);
    CHECK(g_clone_calls==1 && g_destruct_calls==1);
    CHECK(g_free_calls==1 && g_weak_calls==1);
    CHECK(refs[0]==1 && refs[1]==1);
    CHECK(get64(g_source,0x40)==g_source_styles);

    // Rejected clone type is released without returning an unsafe resource.
    g_wrong_clone_type=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::VerificationFailed);
    CHECK(g_destruct_calls==2 && g_free_calls==2 && g_weak_calls==2);
    CHECK(refs[0]==1);
    g_wrong_clone_type=false;

    // A shallow clone of the style array is NOT a verified owning clone.
    g_share_styles=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::VerificationFailed);
    CHECK(g_destruct_calls==3 && g_free_calls==3 && g_weak_calls==3);
    CHECK(get64(g_source,0x40)==g_source_styles);
    g_share_styles=false;

    // An unexpected alias must NEVER decrement the source refcount.
    g_alias_clone=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::ReleaseFailed);
    CHECK(refs[0]==1 && refs[1]==1);
    CHECK(g_destruct_calls==3 && g_free_calls==3 && g_weak_calls==3);
    g_alias_clone=false;

    // Wrong source identity and missing executable release symbol fail closed.
    result=probe_native_ink_clone(source,{kPath,kSourceId+1},cfg);
    CHECK(result.status==WinInkCloneProbeStatus::RejectedSource);
    cfg.dec_weak_reference_function=0;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::RejectedSource);
    CHECK(refs[0]==1 && g_destruct_calls==3);

    ::operator delete(g_source);
    ::operator delete(reinterpret_cast<void*>(g_source_styles));
    delete[] refs;
    std::cout<<"Native clone ABI, strong-handle release and fail-closed alias checks pass\n";
    return 0;
}
