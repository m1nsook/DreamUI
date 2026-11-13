#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/win_load_hook.hpp"
#include "dreamui/ink_load_patch.hpp"
#include "dreamui/address_db.hpp"
#include "dreamui/pe_probe.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <mutex>
#include <span>
#include <string>
#include <vector>
#if defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
#include <MinHook.h>
#endif

namespace dreamui {
namespace {
#if defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
constexpr std::uint32_t kBuildTimestamp=1792368047u;
constexpr std::uint32_t kImageSize=92385280u;
constexpr std::uint32_t kSchedulePostLoadHash=3599579551u;
// Symbol & callback layout verified against the published Codeware native
// source. Game build fingerprint and runtime memory guards are mandatory.
using StageFn=void (__fastcall *)(std::uint64_t,std::uint8_t,std::uint64_t,
                                   std::uint64_t,void**);
StageFn s_original=nullptr;
std::atomic_bool s_installed{false};
std::atomic_bool s_enabled{false};
// Written once before installing the hook, then immutable during engine callbacks.
std::array<InkPaletteEntry,3> s_palette{{
    {"MainColors.Red",{}},
    {"MainColors.PanelBlue",{}},
    {"MainColors.ActiveYellow",{}}
}};
std::atomic<std::uint32_t> s_applied{0};
std::atomic<std::uint32_t> s_failed{0};
std::atomic<std::uint32_t> s_candidates{0};
std::atomic<std::uint32_t> s_restored{0};
std::atomic<std::uint32_t> s_restore_failed{0};
// 0 idle, 1 apply on next validated resource-load callback,
// 2 restore ONLY if the exact same live instance re-enters the callback.
std::atomic<int> s_intent{0};
std::atomic_bool s_journal_active{false};
// The atomic flag protects this entire journal during native callbacks.
InkLoadJournal s_journal{};
std::atomic_flag s_guard=ATOMIC_FLAG_INIT;
std::uintptr_t s_image_base=0;
std::uint32_t s_text_rva=0,s_text_size=0;

bool safe_read(std::uint64_t addr,void* dst,std::size_t size) {
    SIZE_T n=0;
    return addr>=0x10000 && size && size<=4096 &&
        addr<=UINT64_MAX-size &&
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(addr),dst,size,&n) &&
        n==size;
}

bool writable(std::uint64_t address,std::size_t len) {
    if(address<0x10000 || len==0 || address>UINT64_MAX-len)return false;
    MEMORY_BASIC_INFORMATION page{};
    if(VirtualQuery(reinterpret_cast<const void*>(address),&page,sizeof(page))!=sizeof(page))
        return false;
    const auto flags=page.Protect&0xffu;
    if(page.State!=MEM_COMMIT || (page.Protect&PAGE_GUARD))return false;
    if(flags!=PAGE_READWRITE && flags!=PAGE_WRITECOPY &&
       flags!=PAGE_EXECUTE_READWRITE && flags!=PAGE_EXECUTE_WRITECOPY)
        return false;
    const auto begin=reinterpret_cast<std::uintptr_t>(page.BaseAddress);
    return address>=begin && address-begin<=page.RegionSize &&
           len<=page.RegionSize-(address-begin);
}
bool safe_write(std::uint64_t address,const void* src,std::size_t size) {
    if(!writable(address,size))return false;
    SIZE_T n=0;
    return WriteProcessMemory(GetCurrentProcess(),
        reinterpret_cast<void*>(address),src,size,&n) && n==size;
}
std::uint32_t u32(const std::uint8_t* p,std::size_t offset) {
    std::uint32_t x;std::memcpy(&x,p+offset,sizeof(x));return x;
}
std::uint64_t u64(const std::uint8_t* p,std::size_t offset) {
    std::uint64_t x;std::memcpy(&x,p+offset,sizeof(x));return x;
}
bool executable_address(std::uint64_t address) {
    if(address<0x10000)return false;
    if(address<s_image_base+s_text_rva ||
       address>=s_image_base+s_text_rva+s_text_size)return false;
    MEMORY_BASIC_INFORMATION page{};
    if(VirtualQuery(reinterpret_cast<const void*>(address),&page,sizeof(page))!=sizeof(page))
        return false;
    const auto protection=page.Protect&0xffu;
    return page.State==MEM_COMMIT && !(page.Protect&PAGE_GUARD) &&
           (protection==PAGE_EXECUTE || protection==PAGE_EXECUTE_READ ||
            protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY);
}
bool verified_style_type(std::uint64_t object) {
    std::uint64_t vtable=0,method=0;
    if(!safe_read(object,&vtable,sizeof(vtable)) ||
       !safe_read(vtable,&method,sizeof(method)) ||
       !executable_address(method))
        return false;
    // ISerializable::GetNativeType is vtable slot zero in RED4ext.SDK.
    using GetTypeFn=void* (__fastcall *)(void*);
    const auto type=reinterpret_cast<GetTypeFn>(method)(
        reinterpret_cast<void*>(object));
    std::array<std::uint8_t,0x20> meta{};
    return safe_read(reinterpret_cast<std::uintptr_t>(type),meta.data(),meta.size()) &&
           u64(meta.data(),0x18)==cname_hash("inkStyleResource");
}
void handle_serializer(void* serializer) {
    if(!serializer || !s_enabled.load(std::memory_order_relaxed))return;
    std::array<std::uint8_t,0x10> array{};
    const auto addr=reinterpret_cast<std::uintptr_t>(serializer);
    if(!safe_read(addr+0x230,array.data(),array.size()))return;
    const auto handles=u64(array.data(),0);
    const auto capacity=u32(array.data(),8), count=u32(array.data(),12);
    if(!handles || !capacity || count>capacity || count>512)return;
    const MemoryRead read=[](std::uint64_t a,void* d,std::size_t n){
        return safe_read(a,d,n);
    };
    const MemoryWrite write=[](std::uint64_t a,const void* d,std::size_t n){
        return safe_write(a,d,n);
    };
    for(std::uint32_t i=0;i<count;++i) {
        std::array<std::uint8_t,0x10> handle{};
        if(!safe_read(handles+static_cast<std::uint64_t>(i)*0x10,
                      handle.data(),handle.size()))break;
        const auto object=u64(handle.data(),0),refs=u64(handle.data(),8);
        if(object<0x10000 || refs<0x10000)continue;
        std::uint32_t strong=0;
        if(!safe_read(refs,&strong,sizeof(strong)) || !strong)continue;
        if(!verified_style_type(object))continue;
        ++s_candidates;
        std::uint64_t path=0;
        if(!safe_read(object+0x30,&path,sizeof(path)) || !path)continue;
        // The independent 2.5 archive dump confirms main_colors.inkstyle
        // itself contains ten inkStyle entries, ten themes, two overrides.
        // Do NOT indiscriminately change Johnny, color-blindness, or other
        // loaded theme resources. One primary palette first.
        if(path!=resource_path_hash(
            R"(base\gameplay\gui\common\main_colors.inkstyle)"))continue;
        const auto intent=s_intent.load(std::memory_order_acquire);
        if(intent==2) {
            if(!s_journal.active()) {
                s_intent.store(0,std::memory_order_release);
                continue;
            }
            // Crucial: no stored pointer is ever dereferenced unless the
            // game has presented that exact instance under a current strong
            // serializer handle. Other generations are always ignored.
            if(object!=s_journal.resource_address)continue;
            const auto status=restore_loaded_ink_color(
                read,write,object,s_journal);
            if(status==InkLoadRestoreStatus::Restored) {
                s_journal.clear();
                s_journal_active.store(false,std::memory_order_release);
                s_intent.store(0,std::memory_order_release);
                ++s_restored;
            } else if(status==InkLoadRestoreStatus::RollbackFailed ||
                      status==InkLoadRestoreStatus::ChangedExternally ||
                      status==InkLoadRestoreStatus::InvalidResource) {
                ++s_restore_failed;
                // Never blindly write again after identity/layout/rollback
                // validation failed. Keep the original journal for review.
                s_intent.store(0,std::memory_order_release);
                s_enabled.store(false,std::memory_order_release);
            } else if(status==InkLoadRestoreStatus::WriteFailed) {
                ++s_restore_failed; // explicit retry still possible
                s_intent.store(0,std::memory_order_release);
            }
            continue;
        }
        if(intent!=1 || s_journal.active())continue;
        const auto result=patch_loaded_ink_palette(
            read,write,object,path,s_palette,&s_journal);
        if(result.status==InkLoadPatchStatus::Applied) {
            ++s_applied;
            s_journal_active.store(true,std::memory_order_release);
            s_intent.store(0,std::memory_order_release);
        } else if(result.status==InkLoadPatchStatus::RollbackFailed) {
            ++s_failed;
            s_intent.store(0,std::memory_order_release);
            s_enabled.store(false,std::memory_order_release);
        } else if(result.status==InkLoadPatchStatus::InvalidProperty ||
                  result.status==InkLoadPatchStatus::WriteFailed) {
            ++s_failed;
            s_intent.store(0,std::memory_order_release);
        }
    }
}

void __fastcall schedule_hook(std::uint64_t a,std::uint8_t b,std::uint64_t c,
                              std::uint64_t d,void** captures) {
    // Hook runs synchronously in the game's resource serializer scheduling
    // path, NOT on our Tab worker. Its objects are owned by the serializer.
    // Native edits happen before the engine schedules postload jobs.
    if(captures && s_enabled.load(std::memory_order_relaxed) &&
       !s_guard.test_and_set(std::memory_order_acquire)) {
        try {
            void* serializer=nullptr;
            if(safe_read(reinterpret_cast<std::uintptr_t>(captures),
                         &serializer,sizeof(serializer)))
                handle_serializer(serializer);
        } catch(...) {
            ++s_failed;
        }
        s_guard.clear(std::memory_order_release);
    }
    if(s_original)s_original(a,b,c,d,captures);
}
bool get_target_address(const std::filesystem::path& game_exe,
                        std::uint32_t& rva,
                        AddressSections& sections,
                        std::string& error) {
    std::ifstream input(game_exe,std::ios::binary|std::ios::ate);
    if(!input){error="executable unavailable";return false;}
    const auto length=input.tellg();
    if(length<64 || length>1024LL*1024*1024){error="invalid game exe size";return false;}
    input.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if(!input.read(reinterpret_cast<char*>(bytes.data()),length)){
        error="could not read game exe";return false;
    }
    const auto pe=inspect_pe(std::span<const std::uint8_t>(bytes));
    if(!pe.valid || pe.coff_timestamp!=kBuildTimestamp || pe.image_size!=kImageSize){
        error="unsupported game build (native hook disabled)";return false;
    }
    const std::array<std::uint32_t,1> wanted{kSchedulePostLoadHash};
    const auto db=read_address_database(
        game_exe.parent_path()/L"cyberpunk2077_addresses.json",
        pe.coff_timestamp,pe.sections,wanted);
    if(!db.valid){error="linker DB failed: "+db.error;return false;}
    const auto it=db.matches.find(kSchedulePostLoadHash);
    if(it==db.matches.end() || it->second.segment!=1 ||
       it->second.rva>=pe.image_size){
        error="serializer native function not in validated .text section";return false;
    }
    sections=pe.sections;
    rva=it->second.rva;
    return true;
}
#endif // DREAMUI_EXPERIMENTAL_NATIVE_STAGE
} // namespace

bool start_native_resource_stage(
    const std::filesystem::path& game_exe,
    const std::filesystem::path& home,
    const Theme& theme,
    std::ostream& log) noexcept {
#if !defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
    (void)game_exe;(void)home;(void)theme;(void)log;
    return false;
#else
    try {
        // Hard opt-in. Ordinary builds and normal installs are untouched.
        if(!std::filesystem::is_regular_file(home/L"dev"/L"native-ink-preview.enable"))
            return false;
        constexpr std::array<std::string_view,3> tokens{
            "text.primary","text.secondary","selection.active"};
        for(std::size_t i=0;i<tokens.size();++i) {
            const auto found=theme.colors.find(std::string(tokens[i]));
            if(found==theme.colors.end()) {
                log<<"Native Ink preview missing theme token "<<tokens[i]<<"\n";
                return false;
            }
            s_palette[i].theme_color=found->second;
        }
        std::uint32_t rva=0;
        AddressSections sections{};
        std::string error;
        if(!get_target_address(game_exe,rva,sections,error)){
            log<<"Native Ink preview disabled: "<<error<<"\n";return false;
        }
        s_image_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        s_text_rva=sections.text_rva;
        s_text_size=sections.text_size;
        if(!s_image_base ||
           rva>=sections.image_size ||
           !executable_address(s_image_base+rva)){
            log<<"Native Ink preview disabled: invalid executable hook address\n";
            return false;
        }
        if(MH_Initialize()!=MH_OK){
            log<<"Native Ink preview disabled: hook library init failed\n";return false;
        }
        const auto entry=reinterpret_cast<LPVOID>(s_image_base+rva);
        if(MH_CreateHook(entry,reinterpret_cast<LPVOID>(&schedule_hook),
                         reinterpret_cast<LPVOID*>(&s_original))!=MH_OK ||
           !s_original || MH_EnableHook(entry)!=MH_OK){
            MH_RemoveHook(entry);
            MH_Uninitialize();
            log<<"Native Ink preview disabled: guarded hook install failed\n";
            return false;
        }
        s_installed.store(true,std::memory_order_release);
        s_intent.store(1,std::memory_order_release);
        s_enabled.store(true,std::memory_order_release);
        log<<"Experimental standalone Ink serializer stage enabled. "
               "Writes up to 3 verified native palette properties; no DX12 hooks.\n";
        return true;
    }catch(...){
        log<<"Native Ink preview disabled after initialization exception\n";
        return false;
    }
#endif
}
bool native_resource_stage_request_apply() noexcept {
#if !defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
    return false;
#else
    if(!s_installed.load(std::memory_order_acquire) ||
       !s_enabled.load(std::memory_order_acquire) ||
       s_journal_active.load(std::memory_order_acquire))
        return false;
    s_intent.store(1,std::memory_order_release);
    return true;
#endif
}

bool native_resource_stage_request_restore() noexcept {
#if !defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
    return false;
#else
    if(!s_installed.load(std::memory_order_acquire) ||
       !s_enabled.load(std::memory_order_acquire) ||
       !s_journal_active.load(std::memory_order_acquire))
        return false;
    s_intent.store(2,std::memory_order_release);
    return true;
#endif
}

std::string native_resource_stage_status() {
#if !defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
    return "native engine writes=not compiled";
#else
    if(!s_installed.load(std::memory_order_relaxed))
        return "native stage=inactive";
    return std::string("native stage=")+
           (s_enabled.load(std::memory_order_relaxed)?"enabled":"disabled")+
           " candidates="+std::to_string(s_candidates.load())+
           " applied="+std::to_string(s_applied.load())+
           " rejected="+std::to_string(s_failed.load())+
           " journal="+(s_journal_active.load()?"active":"none")+
           " restored="+std::to_string(s_restored.load())+
           " restore_failures="+std::to_string(s_restore_failed.load())+
           " request="+std::to_string(s_intent.load())+
           " (commands deferred until resource lifecycle)";
#endif
}
} // namespace dreamui
