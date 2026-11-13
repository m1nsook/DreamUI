#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/win_clone_probe.hpp"
#include "dreamui/ink_cache.hpp"
#include "dreamui/native_ink.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <vector>

using namespace dreamui;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)

namespace {
constexpr std::uint64_t kPath=0x1234567890ABCDEFULL;
constexpr std::uint64_t kSourceId=0x11223344;
std::array<std::uint8_t,0x20> g_type{};
std::array<std::uint8_t,0x20> g_wrong_type{};
std::array<std::uint8_t,0x78> g_hdr_type{};
std::array<std::uint64_t,27> g_object_vtable{};
std::array<std::uint64_t,5> g_allocator_vtable{};
struct FakeAllocator {std::uint64_t* vtable;};
FakeAllocator g_allocator{g_allocator_vtable.data()};
void* g_source=nullptr;
std::uint64_t g_source_styles=0,g_source_props=0,g_source_color=0;
std::uint64_t g_source_themes=0,g_source_overrides=0;
int g_clone_calls=0,g_destruct_calls=0,g_free_calls=0,g_weak_calls=0;
bool g_wrong_clone_type=false,g_alias_clone=false,g_share_styles=false;
bool g_share_props=false,g_share_color=false;
bool g_share_themes=false,g_share_overrides=false;
struct Unreleased {
    void* instance=nullptr;
    std::uint32_t* refs=nullptr;
};
std::vector<Unreleased> g_unreleased{};
std::uint64_t get64(const void* p,std::size_t off) {
    std::uint64_t x=0;
    std::memcpy(&x,static_cast<const char*>(p)+off,sizeof(x));
    return x;
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
std::uint64_t new_bytes(std::size_t count) {
    auto* p=::operator new(count);
    std::memset(p,0,count);
    return reinterpret_cast<std::uint64_t>(p);
}
void remove_clone_data(void* object) {
    const auto styles=get64(object,0x40);
    if(styles && styles!=g_source_styles) {
        const auto props=get64(reinterpret_cast<const void*>(styles),0x10);
        if(props && props!=g_source_props) {
            const auto color=get64(reinterpret_cast<const void*>(props),0x10);
            if(color && color!=g_source_color)
                delete reinterpret_cast<HdrColor*>(color);
            ::operator delete(reinterpret_cast<void*>(props));
        }
        ::operator delete(reinterpret_cast<void*>(styles));
    }
    const auto themes=get64(object,0x50);
    const auto overrides=get64(object,0x60);
    if(themes && themes!=g_source_themes)
        ::operator delete(reinterpret_cast<void*>(themes));
    if(overrides && overrides!=g_source_overrides)
        ::operator delete(reinterpret_cast<void*>(overrides));
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
    remove_clone_data(object);
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
    std::uint64_t styles=g_share_styles ? g_source_styles : new_bytes(0x40);
    if(!g_share_styles) {
        std::memcpy(reinterpret_cast<void*>(styles),
                    reinterpret_cast<void*>(g_source_styles),0x40);
        const auto props=g_share_props ? g_source_props : new_bytes(0x20);
        if(!g_share_props) {
            std::memcpy(reinterpret_cast<void*>(props),
                        reinterpret_cast<void*>(g_source_props),0x20);
            const auto color=g_share_color ? g_source_color :
                reinterpret_cast<std::uint64_t>(
                    new HdrColor(*reinterpret_cast<HdrColor*>(g_source_color)));
            put64(reinterpret_cast<void*>(props),0x10,color);
        }
        put64(reinterpret_cast<void*>(styles),0x10,props);
    }
    auto* clone=new_resource(0xBB55,styles);
    const auto themes=g_share_themes?g_source_themes:new_bytes(10*0x20);
    if(!g_share_themes)
        std::memcpy(reinterpret_cast<void*>(themes),
                    reinterpret_cast<void*>(g_source_themes),10*0x20);
    put64(clone,0x50,themes);put32(clone,0x58,10);put32(clone,0x5c,10);
    const auto overrides=g_share_overrides?g_source_overrides:new_bytes(2*0x20);
    if(!g_share_overrides)
        std::memcpy(reinterpret_cast<void*>(overrides),
                    reinterpret_cast<void*>(g_source_overrides),2*0x20);
    put64(clone,0x60,overrides);put32(clone,0x68,2);put32(clone,0x6c,2);
    auto* refs=new std::uint32_t[2]{1,1};
    *output={reinterpret_cast<std::uint64_t>(clone),
             reinterpret_cast<std::uint64_t>(refs)};
    if(g_share_styles || g_share_props || g_share_color ||
       g_share_themes || g_share_overrides)
        g_unreleased.push_back({clone,refs});
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
    hash=cname_hash("HDRColor");
    std::memcpy(g_hdr_type.data()+0x18,&hash,8);
    put32(g_hdr_type.data(),0x68,16);
    put32(g_hdr_type.data(),0x74,16);

    g_object_vtable[0]=reinterpret_cast<std::uint64_t>(&get_type);
    g_object_vtable[2]=reinterpret_cast<std::uint64_t>(&get_allocator);
    g_object_vtable[3]=reinterpret_cast<std::uint64_t>(&fake_destroy);
    g_object_vtable[26]=reinterpret_cast<std::uint64_t>(&can_destroy);
    g_allocator_vtable[4]=reinterpret_cast<std::uint64_t>(&allocator_free);

    g_source_styles=new_bytes(0x40);
    g_source_props=new_bytes(0x20);
    g_source_color=reinterpret_cast<std::uint64_t>(
        new HdrColor{1.1761f,0.38f,0.3476f,1.0f});
    put64(reinterpret_cast<void*>(g_source_styles),0,cname_hash("MainColors"));
    put64(reinterpret_cast<void*>(g_source_styles),8,cname_hash("Default"));
    put64(reinterpret_cast<void*>(g_source_styles),0x10,g_source_props);
    put32(reinterpret_cast<void*>(g_source_styles),0x18,1);
    put32(reinterpret_cast<void*>(g_source_styles),0x1C,1);
    put64(reinterpret_cast<void*>(g_source_props),0,cname_hash("MainColors.Red"));
    put64(reinterpret_cast<void*>(g_source_props),8,
          reinterpret_cast<std::uint64_t>(g_hdr_type.data()));
    put64(reinterpret_cast<void*>(g_source_props),0x10,g_source_color);

    g_source=new_resource(kSourceId,g_source_styles);
    // Real main_colors.inkstyle has ten theme refs and two overrides.
    // Fake handles are empty, but their backing arrays must be copied.
    g_source_themes=new_bytes(10*0x20);
    g_source_overrides=new_bytes(2*0x20);
    put64(g_source,0x50,g_source_themes);
    put32(g_source,0x58,10);put32(g_source,0x5c,10);
    put64(g_source,0x60,g_source_overrides);
    put32(g_source,0x68,2);put32(g_source,0x6c,2);
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

    // Metadata failure is safe to clean up once native allocations have
    // been proved independent of every source-owned array and HDRColor.
    g_wrong_clone_type=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::VerificationFailed);
    CHECK(g_destruct_calls==2 && g_free_calls==2 && g_weak_calls==2);
    g_wrong_clone_type=false;

    // Any shared source-owned array or external HDRColor forbids clone
    // destruction. The one-shot leak is safer than freeing source memory.
    g_share_styles=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::ReleaseFailed);
    g_share_styles=false;

    g_share_props=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::ReleaseFailed);
    g_share_props=false;

    g_share_color=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::ReleaseFailed);
    g_share_color=false;

    // Resource theme/override DynArrays also own their storage; merely
    // copying the styles is not enough for safe native destruction.
    g_share_themes=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::ReleaseFailed);
    g_share_themes=false;
    g_share_overrides=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::ReleaseFailed);
    g_share_overrides=false;

    CHECK(g_destruct_calls==2 && g_free_calls==2 && g_weak_calls==2);
    CHECK(refs[0]==1 && refs[1]==1);
    CHECK(get64(g_source,0x40)==g_source_styles);
    CHECK(get64(reinterpret_cast<void*>(g_source_styles),0x10)==g_source_props);
    CHECK(get64(reinterpret_cast<void*>(g_source_props),0x10)==g_source_color);

    // An unexpected clone alias must NEVER decrement the source refcount.
    g_alias_clone=true;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::ReleaseFailed);
    CHECK(refs[0]==1 && refs[1]==1);
    g_alias_clone=false;

    result=probe_native_ink_clone(source,{kPath,kSourceId+1},cfg);
    CHECK(result.status==WinInkCloneProbeStatus::RejectedSource);
    cfg.dec_weak_reference_function=0;
    result=probe_native_ink_clone(source,identity,cfg);
    CHECK(result.status==WinInkCloneProbeStatus::RejectedSource);
    CHECK(refs[0]==1 && g_destruct_calls==2);

    // Manually recover rejected test-only aliases using our fake memory
    // manager. The production backend intentionally cannot do this.
    for(auto& leak:g_unreleased) {
        remove_clone_data(leak.instance);
        ::operator delete(leak.instance);
        delete[] leak.refs;
    }
    ::operator delete(g_source);
    ::operator delete(reinterpret_cast<void*>(g_source_styles));
    ::operator delete(reinterpret_cast<void*>(g_source_props));
    delete reinterpret_cast<HdrColor*>(g_source_color);
    ::operator delete(reinterpret_cast<void*>(g_source_themes));
    ::operator delete(reinterpret_cast<void*>(g_source_overrides));
    delete[] refs;
    std::cout<<"Clone/Handle release ABI and nested allocation ownership checks pass\n";
    return 0;
}
