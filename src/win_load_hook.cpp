#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dreamui/win_load_hook.hpp"
#include "dreamui/ink_load_patch.hpp"
#include "dreamui/ink_style_snapshot.hpp"
#include "dreamui/native_ink_clone.hpp"
#include "dreamui/win_clone_probe.hpp"
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
#include <utility>
#if defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
#include <MinHook.h>
#endif

namespace dreamui {
namespace {
#if defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
constexpr std::uint32_t kBuildTimestamp=1792368047u;
constexpr std::uint32_t kImageSize=92385280u;
constexpr std::uint32_t kSchedulePostLoadHash=3599579551u;
// RED4ext.SDK Handle_DecWeakRef, used only for releasing a game-owned clone.
constexpr std::uint32_t kHandleDecWeakHash=0x333B1404u;
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
std::mutex s_palette_lock;
// Value-only first-load capture. Do not store engine allocation addresses.
// The serializer callback and theme selector use the same palette lock.
InkOwnedColorPreimage s_first_color_preimage{};
// A complete, value-owned style tree is retained only if EVERY native
// Variant is an authenticated HDRColor. No foreign allocator pointers.
InkOwnedStyleTree s_full_style_tree{};
std::atomic<std::uint32_t> s_full_tree_attempts{0};
std::atomic<int> s_full_tree_status{-1};
std::atomic<std::uint32_t> s_full_tree_observed_properties{0};
std::atomic<std::uint32_t> s_full_tree_unsupported{0};
std::atomic<std::uint32_t> s_full_tree_styles{0};
std::atomic<std::uint32_t> s_full_tree_colors{0};
// Symbol lookup alone never authorizes a native clone call or lifetime
// extension; proper game-engine Handle release is still unwired.
std::atomic_bool s_clone_symbol_resolved{false};
std::atomic_bool s_clone_release_symbol_resolved{false};
std::atomic_bool s_clone_requested{false};
std::atomic_bool s_clone_attempted{false};
std::atomic<int> s_clone_outcome{static_cast<int>(WinInkCloneProbeStatus::NotAttempted)};
std::atomic<std::uint32_t> s_clone_observed_styles{0};
WinInkCloneProbeConfig s_clone_cfg{};
std::atomic<std::uint32_t> s_owned_color_entries{0};
std::atomic<std::uint32_t> s_owned_source_styles{0};
std::atomic<std::uint32_t> s_applied{0};
std::atomic<std::uint32_t> s_failed{0};
std::atomic<std::uint32_t> s_candidates{0};
// Counts real serializer callback entries even when no target was found.
std::atomic<std::uint32_t> s_serializer_callbacks{0};
// Read-only telemetry for the authenticated main-colors resource. All
// counters are safe to sample from the developer worker without locking.
std::atomic<std::uint32_t> s_targets{0};
// Per-target observation captured BEFORE the engine's post-load work.
// These are counters only; never retain raw engine object addresses.
std::atomic<std::uint32_t> s_last_target_styles{0};
std::atomic<std::uint32_t> s_peak_target_styles{0};
std::atomic<std::uint32_t> s_last_matched_properties{0};
std::atomic<std::uint32_t> s_preflight_ready{0};
std::atomic<std::uint32_t> s_preflight_no_styles{0};
std::atomic<std::uint32_t> s_preflight_missing{0};
std::atomic<std::uint32_t> s_preflight_rejected{0};
// An experimental hook does not imply permission to mutate native Ink.
// A second explicit marker is required before any write intent is allowed.
std::atomic_bool s_write_mode{false};
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
const char* clone_probe_status_name(int status) noexcept {
    switch(static_cast<WinInkCloneProbeStatus>(status)) {
        case WinInkCloneProbeStatus::NotAttempted: return "not_attempted";
        case WinInkCloneProbeStatus::RejectedSource: return "rejected_source";
        case WinInkCloneProbeStatus::CloneFailed: return "clone_failed";
        case WinInkCloneProbeStatus::VerificationFailed: return "verification_failed";
        case WinInkCloneProbeStatus::Released: return "verified_and_released";
        case WinInkCloneProbeStatus::ReleaseFailed: return "release_failed";
    }
    return "unknown";
}
const char* style_capture_status_name(int status) noexcept {
    switch(static_cast<InkStyleCaptureStatus>(status)) {
        case InkStyleCaptureStatus::Captured: return "captured";
        case InkStyleCaptureStatus::NoStyles: return "no_styles";
        case InkStyleCaptureStatus::InvalidVariant: return "invalid_variant";
        case InkStyleCaptureStatus::UnsupportedVariant: return "unsupported_variant";
        case InkStyleCaptureStatus::LimitExceeded: return "limit_exceeded";
        case InkStyleCaptureStatus::InvalidResource: return "invalid_resource";
    }
    return "not_attempted";
}
void maybe_capture_full_style_tree(const MemoryRead& read,
                                   std::uint64_t object,std::uint64_t path) {
    // Called under s_palette_lock after serializer RTTI, strong-handle,
    // and exact resource path checks. Never copy a native DynArray header:
    // all copied styles/properties are ordinary DreamUI-owned C++ values.
    if(s_full_style_tree.complete())return;
    InkOwnedStyleTree candidate{};
    const auto result=capture_loaded_ink_style_tree(read,object,path,candidate);
    ++s_full_tree_attempts;
    s_full_tree_status.store(static_cast<int>(result.status),
                             std::memory_order_release);
    s_full_tree_observed_properties.store(result.observed_properties,
                                          std::memory_order_release);
    s_full_tree_unsupported.store(result.unsupported_properties,
                                  std::memory_order_release);
    if(result.status==InkStyleCaptureStatus::Captured && candidate.complete()) {
        s_full_tree_styles.store(static_cast<std::uint32_t>(candidate.styles.size()),
                                 std::memory_order_release);
        s_full_tree_colors.store(static_cast<std::uint32_t>(candidate.properties.size()),
                                 std::memory_order_release);
        s_full_style_tree=std::move(candidate);
    }
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
        ++s_targets;
        if(!s_write_mode.load(std::memory_order_acquire)) {
            // The hook is in the game's serializer callback, with a verified
            // strong handle, but observation must never write native memory.
            std::unique_lock palette_guard(s_palette_lock,std::try_to_lock);
            if(!palette_guard.owns_lock())continue;
            InkOwnedColorPreimage captured{};
            const auto inspection=inspect_loaded_ink_palette(
                read,object,path,s_palette,&captured);
            if(captured.valid() && !s_first_color_preimage.valid()) {
                s_first_color_preimage=captured;
                s_owned_color_entries.store(captured.entry_count,std::memory_order_release);
                s_owned_source_styles.store(captured.style_count,std::memory_order_release);
            }
            maybe_capture_full_style_tree(read,object,path);
            s_last_target_styles.store(inspection.styles,std::memory_order_relaxed);
            s_last_matched_properties.store(inspection.matched,std::memory_order_relaxed);
            if(inspection.styles>s_peak_target_styles.load(std::memory_order_relaxed))
                s_peak_target_styles.store(inspection.styles,std::memory_order_relaxed);
            if(inspection.eligible) {
                ++s_preflight_ready;
                // An additional explicit marker authorizes exactly ONE
                // native clone, released in this same engine callback.
                // Never invoked by the Tab diagnostic worker.
                if(s_clone_requested.load(std::memory_order_acquire) &&
                   !s_clone_attempted.exchange(true,std::memory_order_acq_rel)) {
                    std::uint64_t resource_id=0;
                    if(safe_read(object+0x28,&resource_id,sizeof(resource_id)) &&
                       resource_id) {
                        const auto probe=probe_native_ink_clone(
                            InkBorrowedHandle{object,refs},
                            InkCloneIdentity{path,resource_id},s_clone_cfg);
                        s_clone_observed_styles.store(probe.cloned_styles,
                                                       std::memory_order_release);
                        s_clone_outcome.store(static_cast<int>(probe.status),
                                              std::memory_order_release);
                    } else {
                        s_clone_outcome.store(static_cast<int>(
                            WinInkCloneProbeStatus::RejectedSource),
                            std::memory_order_release);
                    }
                }
            }
            else if(inspection.failure==InkLoadPatchStatus::NoStyles)
                ++s_preflight_no_styles;
            else if(inspection.failure==InkLoadPatchStatus::NoMatchingProperty)
                ++s_preflight_missing;
            else ++s_preflight_rejected;
            continue;
        }
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
        // Keep the palette lock through the transaction AND journal state
        // update. Otherwise a theme change could race after the snapshot
        // but before the game callback marks the journal active.
        std::unique_lock palette_guard(s_palette_lock,std::try_to_lock);
        if(!palette_guard.owns_lock())continue;
        // Capture vanilla HDRColor values before any native writer runs.
        // This is a VALUE snapshot only, not an owning inkStyle array.
        if(!s_first_color_preimage.valid()) {
            InkOwnedColorPreimage captured{};
            const auto inspected=inspect_loaded_ink_palette(
                read,object,path,s_palette,&captured);
            if(inspected.eligible && captured.valid()) {
                s_first_color_preimage=captured;
                s_owned_color_entries.store(captured.entry_count,std::memory_order_release);
                s_owned_source_styles.store(captured.style_count,std::memory_order_release);
            }
        }
        // Attempt full style-tree capture BEFORE any possible native writer.
        // A type we cannot safely copy prevents complete capture.
        maybe_capture_full_style_tree(read,object,path);
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
    if(s_enabled.load(std::memory_order_relaxed))++s_serializer_callbacks;
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
                        std::uint32_t& optional_clone_rva,
                        std::uint32_t& optional_weak_rva,
                        AddressSections& sections,
                        std::string& error) {
    optional_clone_rva=0;
    optional_weak_rva=0;
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
    // Codeware's independently published ISerializable::Clone address
    // hash is optional; the existing validated hook remains required.
    const std::array<std::uint32_t,3> wanted{
        kSchedulePostLoadHash,kNativeISerializableCloneHash,kHandleDecWeakHash};
    const auto db=read_address_database(
        game_exe.parent_path()/L"cyberpunk2077_addresses.json",
        pe.coff_timestamp,pe.sections,wanted);
    if(!db.valid){error="linker DB failed: "+db.error;return false;}
    const auto it=db.matches.find(kSchedulePostLoadHash);
    if(it==db.matches.end() || it->second.segment!=1 ||
       it->second.rva>=pe.image_size){
        error="serializer native function not in validated .text section";return false;
    }
    const auto clone=db.matches.find(kNativeISerializableCloneHash);
    if(clone!=db.matches.end() && clone->second.segment==1 &&
       clone->second.rva>=pe.sections.text_rva &&
       clone->second.rva-pe.sections.text_rva<pe.sections.text_size &&
       clone->second.rva!=it->second.rva)
        optional_clone_rva=clone->second.rva;
    const auto weak=db.matches.find(kHandleDecWeakHash);
    if(weak!=db.matches.end() && weak->second.segment==1 &&
       weak->second.rva>=pe.sections.text_rva &&
       weak->second.rva-pe.sections.text_rva<pe.sections.text_size &&
       weak->second.rva!=it->second.rva)
        optional_weak_rva=weak->second.rva;
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
        if(!native_resource_stage_set_theme(theme)) {
            log<<"Native Ink preview missing color tokens or journal active\n";
            return false;
        }
        std::uint32_t rva=0,clone_rva=0,weak_rva=0;
        AddressSections sections{};
        std::string error;
        if(!get_target_address(game_exe,rva,clone_rva,weak_rva,sections,error)){
            log<<"Native Ink preview disabled: "<<error<<"\n";return false;
        }
        s_image_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        s_text_rva=sections.text_rva;
        s_text_size=sections.text_size;
        if(s_image_base && clone_rva)
            s_clone_symbol_resolved.store(
                executable_address(s_image_base+clone_rva),
                std::memory_order_release);
        if(s_image_base && weak_rva)
            s_clone_release_symbol_resolved.store(
                executable_address(s_image_base+weak_rva),
                std::memory_order_release);
        s_clone_cfg={
            s_image_base+clone_rva,s_image_base+weak_rva,
            s_image_base+s_text_rva,s_image_base+s_text_rva+s_text_size
        };
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
        // Determine marker permissions once. The clone marker is
        // independent: it allocates and releases a temporary engine object,
        // and is explicitly disallowed when native writes are enabled.
        // Shipping alpha.9 has neither permission compiled in. Marker
        // files ALONE cannot enable native color writes or Clone allocation.
#if defined(DREAMUI_ENABLE_NATIVE_WRITES)
        const bool write_mode=std::filesystem::is_regular_file(
            home/L"dev"/L"native-ink-write.enable");
#else
        const bool write_mode=false;
#endif
#if defined(DREAMUI_ENABLE_NATIVE_CLONE)
        const bool clone_mode=!write_mode &&
            std::filesystem::is_regular_file(
                home/L"dev"/L"native-ink-clone.enable") &&
            s_clone_symbol_resolved.load(std::memory_order_acquire) &&
            s_clone_release_symbol_resolved.load(std::memory_order_acquire);
#else
        const bool clone_mode=false;
#endif
        s_clone_requested.store(clone_mode,std::memory_order_release);
        s_write_mode.store(write_mode,std::memory_order_release);
        s_installed.store(true,std::memory_order_release);
        s_intent.store(write_mode?1:0,std::memory_order_release);
        s_enabled.store(true,std::memory_order_release);
        if(write_mode)
            log<<"Experimental native Ink write mode explicitly enabled. "
                   "Up to 3 palette properties may change at load; no UI refresh.\n";
        else if(clone_mode)
            log<<"Experimental one-shot native Ink clone-and-release armed. "
                   "No palette writes; Clone allocates engine memory, "
                   "so this is NOT a read-only preflight.\n";
        else
            log<<"Experimental native Ink read-only preflight enabled. "
                   "No native Ink color writes (hook installation still patches code); automatic trace is DreamUI-load.log.\n";
        return true;
    }catch(...){
        log<<"Native Ink preview disabled after initialization exception\n";
        return false;
    }
#endif
}
bool native_resource_stage_set_theme(const Theme& theme) noexcept {
#if !defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
    (void)theme;
    return true; // theme selection is independent of experimental engine hooks
#else
    if(s_journal_active.load(std::memory_order_acquire))return false;
    constexpr std::array<std::string_view,3> tokens{
        "text.primary","text.secondary","selection.active"};
    std::array<InkPaletteEntry,3> selected{{
        {"MainColors.Red",{}},{"MainColors.PanelBlue",{}},
        {"MainColors.ActiveYellow",{}}
    }};
    for(std::size_t i=0;i<tokens.size();++i){
        const auto found=theme.colors.find(std::string(tokens[i]));
        if(found==theme.colors.end())return false;
        selected[i].theme_color=found->second;
    }
    try {
        std::lock_guard guard(s_palette_lock);
        if(s_journal_active.load(std::memory_order_acquire))return false;
        s_palette=selected;
    } catch(...) {return false;}
    return true;
#endif
}

bool native_resource_stage_request_apply() noexcept {
#if !defined(DREAMUI_EXPERIMENTAL_NATIVE_STAGE)
    return false;
#else
    if(!s_installed.load(std::memory_order_acquire) ||
       !s_enabled.load(std::memory_order_acquire) ||
       !s_write_mode.load(std::memory_order_acquire) ||
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
       !s_write_mode.load(std::memory_order_acquire) ||
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
           (s_write_mode.load(std::memory_order_relaxed)?" mode=write":
            s_clone_requested.load(std::memory_order_relaxed)?" mode=clone-test":" mode=observe-only")+
           " serializer_callbacks="+std::to_string(s_serializer_callbacks.load())+
           " candidates="+std::to_string(s_candidates.load())+
           " last_target_styles="+std::to_string(s_last_target_styles.load())+
           " peak_target_styles="+std::to_string(s_peak_target_styles.load())+
           " last_matched_properties="+std::to_string(s_last_matched_properties.load())+
           " owned_original_colors="+std::to_string(s_owned_color_entries.load())+
           " source_styles="+std::to_string(s_owned_source_styles.load())+
           " full_tree_capture="+style_capture_status_name(s_full_tree_status.load())+
           " full_tree_attempts="+std::to_string(s_full_tree_attempts.load())+
           " full_tree_styles="+std::to_string(s_full_tree_styles.load())+
           " full_tree_colors="+std::to_string(s_full_tree_colors.load())+
           " full_tree_inspected="+std::to_string(s_full_tree_observed_properties.load())+
           " full_tree_unsupported="+std::to_string(s_full_tree_unsupported.load())+
           " clone_symbol="+(s_clone_symbol_resolved.load()?"resolved":"unavailable")+
           " clone_release_symbol="+(s_clone_release_symbol_resolved.load()?"resolved":"unavailable")+
           " write_backend="+
#if defined(DREAMUI_ENABLE_NATIVE_WRITES)
           std::string("compiled")+
#else
           std::string("locked_out")+
#endif
           " clone_backend="+
#if defined(DREAMUI_ENABLE_NATIVE_CLONE)
           std::string("compiled")+
#else
           std::string("locked_out")+
#endif
           " clone_probe="+clone_probe_status_name(s_clone_outcome.load())+
           " clone_attempted="+std::to_string(s_clone_attempted.load()?1:0)+
           " clone_styles="+std::to_string(s_clone_observed_styles.load())+
           " target_resources="+std::to_string(s_targets.load())+
           " preflight_ready="+std::to_string(s_preflight_ready.load())+
           " preflight_no_styles="+std::to_string(s_preflight_no_styles.load())+
           " preflight_missing="+std::to_string(s_preflight_missing.load())+
           " preflight_rejected="+std::to_string(s_preflight_rejected.load())+
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
