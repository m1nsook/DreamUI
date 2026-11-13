#pragma once
#include "dreamui/native_ink_clone.hpp"
#include <cstdint>

namespace dreamui {

// Windows-only, explicitly gated experiment. No game mutation is performed
// to the source inkStyleResource; Clone itself allocates native engine memory.
// The output native Handle is validated and released before this returns.
enum class WinInkCloneProbeStatus {
    NotAttempted,
    RejectedSource,
    CloneFailed,
    VerificationFailed,
    Released,
    ReleaseFailed
};
struct WinInkCloneProbeResult {
    WinInkCloneProbeStatus status=WinInkCloneProbeStatus::NotAttempted;
    std::uint32_t cloned_styles=0;
    std::uint32_t source_styles=0;
};
struct WinInkCloneProbeConfig {
    std::uint64_t clone_function=0;
    std::uint64_t dec_weak_reference_function=0;
    std::uint64_t executable_text_begin=0;
    std::uint64_t executable_text_end=0;
};

// Call ONLY while the authentic serializer strong Handle is live on the
// engine's callback thread and BEFORE scheduling PostLoad jobs. The caller
// must validate source RTTI, path and refcount first, and arm this function
// through a separate test-only marker. This never retains a native handle,
// invokes PostLoad or refreshes any live UI.
WinInkCloneProbeResult probe_native_ink_clone(
    InkBorrowedHandle live_source,
    InkCloneIdentity source_identity,
    const WinInkCloneProbeConfig& config) noexcept;

} // namespace dreamui
