#include "dreamui/native_ink_clone.hpp"
#include <utility>

namespace dreamui {

InkCloneStatus InkOwnedCloneSession::capture(
    InkBorrowedHandle source,InkCloneIdentity identity) noexcept {
    if(active())return InkCloneStatus::AlreadyOwned;
    if(!identity.path_hash || !identity.source_instance_id ||
       source.instance<0x10000 || source.ref_count<0x10000)
        return InkCloneStatus::InvalidSource;
    if(!ops_.in_owning_callback())
        return InkCloneStatus::WrongContext;
    if(!ops_.verify_source(source,identity))
        return InkCloneStatus::InvalidSource;

    InkOwnedNativeHandle proposed{};
    const bool cloned=ops_.clone_owned(source,proposed);
    if(!cloned || !proposed.active() ||
       proposed.instance==source.instance ||
       proposed.ref_count==source.ref_count) {
        // Even a failed engine clone can return a partially initialized
        // strong handle. Delegate disposal to the native Handle backend.
        if(proposed.instance || proposed.ref_count)
            ops_.release_owned(proposed);
        return InkCloneStatus::CloneFailed;
    }
    if(!ops_.verify_clone(proposed,identity)) {
        ops_.release_owned(proposed);
        return InkCloneStatus::VerificationFailed;
    }

    owned_=std::exchange(proposed,{});
    identity_=identity;
    return InkCloneStatus::Owned;
}

void InkOwnedCloneSession::reset() noexcept {
    if(owned_.instance || owned_.ref_count)
        ops_.release_owned(owned_);
    owned_={};
    identity_={};
}
} // namespace dreamui
