#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/win_clone_probe.hpp"
#include "dreamui/ink_cache.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dreamui {
namespace {
constexpr std::uint32_t kMaxStyles=32;
std::uint32_t u32(const std::uint8_t* p,std::size_t off) {
    std::uint32_t v=0;std::memcpy(&v,p+off,sizeof(v));return v;
}
std::uint64_t u64(const std::uint8_t* p,std::size_t off) {
    std::uint64_t v=0;std::memcpy(&v,p+off,sizeof(v));return v;
}
bool read_memory(std::uint64_t at,void* into,std::size_t n) noexcept {
    SIZE_T done=0;
    return at>=0x10000 && at<=UINT64_MAX-n && n && n<=4096 &&
           ReadProcessMemory(GetCurrentProcess(),
               reinterpret_cast<const void*>(at),into,n,&done) &&
           done==n;
}
bool executable_in_game(std::uint64_t at,const WinInkCloneProbeConfig& cfg) noexcept {
    if(at<cfg.executable_text_begin || at>=cfg.executable_text_end)return false;
    MEMORY_BASIC_INFORMATION page{};
    if(VirtualQuery(reinterpret_cast<const void*>(at),&page,sizeof(page))!=sizeof(page))
        return false;
    const auto prot=page.Protect&0xffu;
    return page.State==MEM_COMMIT && !(page.Protect&PAGE_GUARD) &&
           (prot==PAGE_EXECUTE || prot==PAGE_EXECUTE_READ ||
            prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY);
}
bool writable_refcount(std::uint64_t at) noexcept {
    if(at<0x10000 || at>UINT64_MAX-8)return false;
    MEMORY_BASIC_INFORMATION page{};
    if(VirtualQuery(reinterpret_cast<const void*>(at),&page,sizeof(page))!=sizeof(page))
        return false;
    const auto prot=page.Protect&0xffu;
    const auto base=reinterpret_cast<std::uintptr_t>(page.BaseAddress);
    return page.State==MEM_COMMIT && !(page.Protect&PAGE_GUARD) &&
           (prot==PAGE_READWRITE || prot==PAGE_WRITECOPY ||
            prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY) &&
           at>=base && at-base<=page.RegionSize && 8<=page.RegionSize-(at-base);
}
bool vmethod(std::uint64_t instance,std::uint32_t slot,
             const WinInkCloneProbeConfig& cfg,
             std::uint64_t& method) noexcept {
    std::uint64_t vtable=0;
    if(!read_memory(instance,&vtable,sizeof(vtable)) || vtable<0x10000 ||
       !read_memory(vtable+static_cast<std::uint64_t>(slot)*8,
                    &method,sizeof(method)))return false;
    return executable_in_game(method,cfg);
}
struct StyleArray {
    std::uint64_t data=0;
    std::uint32_t count=0;
};
bool read_style_array(std::uint64_t resource,StyleArray& array) noexcept {
    std::array<std::uint8_t,0x50> head{};
    if(!read_memory(resource,head.data(),head.size()))return false;
    const auto data=u64(head.data(),0x40);
    const auto capacity=u32(head.data(),0x48),count=u32(head.data(),0x4c);
    if(!data || data<0x10000 || !count || count>kMaxStyles || count>capacity)
        return false;
    std::array<std::uint8_t,0x20> first{};
    if(!read_memory(data,first.data(),first.size()))return false;
    array={data,count};
    return true;
}
bool type_and_path(std::uint64_t object,std::uint64_t path,
                   const WinInkCloneProbeConfig& cfg,
                   std::uint64_t& instance_id) noexcept {
    std::uint64_t method=0;
    if(!vmethod(object,0,cfg,method))return false;
    using GetTypeFn=void* (__fastcall*)(void*);
    const auto type=reinterpret_cast<GetTypeFn>(method)(
        reinterpret_cast<void*>(object));
    std::array<std::uint8_t,0x20> meta{};
    if(!read_memory(reinterpret_cast<std::uint64_t>(type),
                    meta.data(),meta.size()) ||
       u64(meta.data(),0x18)!=cname_hash("inkStyleResource"))return false;
    std::array<std::uint8_t,0x38> head{};
    if(!read_memory(object,head.data(),head.size()) ||
       u64(head.data(),0x30)!=path)return false;
    instance_id=u64(head.data(),0x28);
    return instance_id!=0;
}

struct AllocationResult { void* memory; std::size_t size; };
static_assert(sizeof(AllocationResult)==0x10);

// Follows the destruction sequence of RED4ext.SDK Handle<ISerializable>:
// RefCnt::DecRef -> Handle_DecWeakRef (only on last strong ref) ->
// CanBeDestructed -> Memory::Delete(GetAllocator(), instance).
// A vtable slot is called ONLY after checking the game's .text section.
// If validation fails BEFORE decrement, the handle is intentionally not
// touched rather than guessing an allocator/destructor on game memory.
bool release_native_handle(InkOwnedNativeHandle& owned,
                           const WinInkCloneProbeConfig& cfg) noexcept {
    if(!owned.active() || !writable_refcount(owned.ref_count))return false;
    std::array<std::uint32_t,2> refs{};
    if(!read_memory(owned.ref_count,refs.data(),sizeof(refs)) ||
       refs[0]==0 || refs[1]==0 ||
       !executable_in_game(cfg.dec_weak_reference_function,cfg))
        return false;

    std::uint64_t get_allocator=0,destructor=0,can_destroy=0;
    if(!vmethod(owned.instance,2,cfg,get_allocator) ||
       !vmethod(owned.instance,3,cfg,destructor) ||
       !vmethod(owned.instance,26,cfg,can_destroy))
        return false;
    using GetAllocatorFn=void* (__fastcall*)(void*);
    using CanDestroyFn=bool (__fastcall*)(void*);
    void* const object=reinterpret_cast<void*>(owned.instance);
    // Capture the engine allocator and destroy permission BEFORE releasing
    // the last reference so every required call can be validated first.
    void* const allocator=reinterpret_cast<GetAllocatorFn>(get_allocator)(object);
    if(!allocator)return false;
    std::uint64_t free_method=0;
    if(!vmethod(reinterpret_cast<std::uint64_t>(allocator),4,cfg,free_method))
        return false;
    const bool allowed_to_destroy=
        reinterpret_cast<CanDestroyFn>(can_destroy)(object);

    auto* strong=reinterpret_cast<volatile LONG*>(owned.ref_count);
    const LONG old=InterlockedExchangeAdd(strong,-1);
    if(old<=0) {
        InterlockedExchangeAdd(strong,1);
        return false;
    }
    if(old==1) {
        using DecWeakFn=void (__fastcall*)(InkOwnedNativeHandle*);
        reinterpret_cast<DecWeakFn>(cfg.dec_weak_reference_function)(&owned);
        if(allowed_to_destroy) {
            // MSVC virtual deleting destructor, flag=0: destroy only.
            // The SDK invokes the virtual destructor then allocator->Free.
            using DestroyOnlyFn=void (__fastcall*)(void*,unsigned);
            reinterpret_cast<DestroyOnlyFn>(destructor)(object,0);
            AllocationResult allocation{object,0};
            using FreeFn=void (__fastcall*)(void*,AllocationResult*);
            reinterpret_cast<FreeFn>(free_method)(allocator,&allocation);
        }
    }
    owned={};
    return true;
}

class WindowsCloneBackend final : public NativeInkCloneOps {
public:
    WindowsCloneBackend(InkBorrowedHandle source,const WinInkCloneProbeConfig& config,
                        WinInkCloneProbeResult& result) noexcept
        :source_(source),cfg_(config),result_(result){}
    bool in_owning_callback() const noexcept override {return true;}
    bool verify_source(InkBorrowedHandle source,InkCloneIdentity expected) noexcept override {
        if(source.instance!=source_.instance || source.ref_count!=source_.ref_count ||
           !writable_refcount(source.ref_count))return false;
        std::array<std::uint32_t,2> refs{};
        if(!read_memory(source.ref_count,refs.data(),sizeof(refs)) ||
           !refs[0] || !refs[1])return false;
        std::uint64_t identity=0;
        if(!type_and_path(source.instance,expected.path_hash,cfg_,identity) ||
           identity!=expected.source_instance_id ||
           !read_style_array(source.instance,source_styles_))return false;
        result_.source_styles=source_styles_.count;
        return true;
    }
    bool clone_owned(InkBorrowedHandle source,InkOwnedNativeHandle& out) noexcept override {
        if(!executable_in_game(cfg_.clone_function,cfg_) ||
           !executable_in_game(cfg_.dec_weak_reference_function,cfg_))
            return false;
        using CloneFn=void* (__fastcall*)(InkOwnedNativeHandle*,
                                          const InkBorrowedHandle*);
        // Matches Codeware Raw::ISerializable::Clone(Handle&, const Handle&).
        // Only the game's implementation constructs this owning handle.
        reinterpret_cast<CloneFn>(cfg_.clone_function)(&out,&source);
        return out.active();
    }
    bool verify_clone(const InkOwnedNativeHandle& clone,
                      InkCloneIdentity identity) noexcept override {
        if(!clone.active() || !writable_refcount(clone.ref_count) ||
           clone.instance==source_.instance || clone.ref_count==source_.ref_count)
            return false;
        std::array<std::uint32_t,2> refs{};
        if(!read_memory(clone.ref_count,refs.data(),sizeof(refs)) ||
           !refs[0] || !refs[1])return false;
        std::uint64_t cloned_id=0;
        if(!type_and_path(clone.instance,identity.path_hash,cfg_,cloned_id))
            return false;
        StyleArray cloned{};
        if(!read_style_array(clone.instance,cloned) ||
           cloned.count!=source_styles_.count ||
           cloned.data==source_styles_.data)return false;
        result_.cloned_styles=cloned.count;
        return true;
    }
    void release_owned(InkOwnedNativeHandle& out) noexcept override {
        // If the engine unexpectedly returned an alias, we cannot assume
        // that Clone incremented a new strong reference. Never decrement
        // the original resource's serializer-owned refcount on a guess.
        if(out.instance==source_.instance || out.ref_count==source_.ref_count ||
           !release_native_handle(out,cfg_))
            release_failed_=true;
    }
    bool release_failed() const noexcept {return release_failed_;}
private:
    InkBorrowedHandle source_{};
    WinInkCloneProbeConfig cfg_{};
    WinInkCloneProbeResult& result_;
    StyleArray source_styles_{};
    bool release_failed_=false;
};
} // namespace

WinInkCloneProbeResult probe_native_ink_clone(
    InkBorrowedHandle source,InkCloneIdentity identity,
    const WinInkCloneProbeConfig& cfg) noexcept {
    WinInkCloneProbeResult result;
    if(!executable_in_game(cfg.clone_function,cfg) ||
       !executable_in_game(cfg.dec_weak_reference_function,cfg)) {
        result.status=WinInkCloneProbeStatus::RejectedSource;
        return result;
    }
    WindowsCloneBackend backend(source,cfg,result);
    // The owner never outlives its backend. In particular, the clone is
    // released while the serializer callback still holds the source.
    InkOwnedCloneSession session(backend);
    const auto status=session.capture(source,identity);
    if(status==InkCloneStatus::Owned) {
        session.reset();
        result.status=backend.release_failed()
            ? WinInkCloneProbeStatus::ReleaseFailed
            : WinInkCloneProbeStatus::Released;
    } else if(backend.release_failed()) {
        result.status=WinInkCloneProbeStatus::ReleaseFailed;
    } else if(status==InkCloneStatus::VerificationFailed) {
        result.status=WinInkCloneProbeStatus::VerificationFailed;
    } else if(status==InkCloneStatus::CloneFailed) {
        result.status=WinInkCloneProbeStatus::CloneFailed;
    } else {
        result.status=WinInkCloneProbeStatus::RejectedSource;
    }
    return result;
}
} // namespace dreamui
