#include "dreamui/native_ink_clone.hpp"
#include <cstdint>
#include <iostream>

using namespace dreamui;
#define CHECK(x) do {if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(false)

struct FakeCloneBackend final : NativeInkCloneOps {
    bool in_callback=true,source_ok=true,clone_ok=true,verify_ok=true;
    bool returned_partial=false,clone_same_source=false,clone_same_ref=false;
    int clone_calls=0,release_calls=0,verify_source_calls=0,verify_clone_calls=0;
    int created=0,disposed=0;
    bool in_owning_callback() const noexcept override {return in_callback;}
    bool verify_source(InkBorrowedHandle source,InkCloneIdentity id) noexcept override {
        ++verify_source_calls;
        return source_ok && source.instance>=0x10000 &&
               source.ref_count>=0x10000 &&
               id.path_hash==0xAA55 && id.source_instance_id==0x11;
    }
    bool clone_owned(InkBorrowedHandle source,InkOwnedNativeHandle& out) noexcept override {
        ++clone_calls;
        if(returned_partial) {
            out={0x530000,0x540000}; ++created;
            return false;
        }
        if(!clone_ok)return false;
        out={clone_same_source?source.instance:0x530000,
             clone_same_ref?source.ref_count:0x540000};
        ++created;
        return true;
    }
    bool verify_clone(const InkOwnedNativeHandle& owned,InkCloneIdentity id) noexcept override {
        ++verify_clone_calls;
        return verify_ok && owned.instance!=0 && owned.ref_count!=0 &&
               id.path_hash==0xAA55 && id.source_instance_id==0x11;
    }
    void release_owned(InkOwnedNativeHandle& owned) noexcept override {
        if(owned.instance||owned.ref_count){++release_calls;++disposed;}
        owned={};
    }
};

int main() {
    constexpr InkBorrowedHandle source{0x310000,0x320000};
    constexpr InkCloneIdentity identity{0xAA55,0x11};
    static_assert(sizeof(InkBorrowedHandle)==16);
    static_assert(sizeof(InkOwnedNativeHandle)==16);
    static_assert(kNativeISerializableCloneHash==2601784524u);
    FakeCloneBackend backend;

    {
        InkOwnedCloneSession session(backend);
        CHECK(session.capture({},identity)==InkCloneStatus::InvalidSource);
        CHECK(session.capture(source,{})==InkCloneStatus::InvalidSource);
        CHECK(backend.clone_calls==0);
        backend.in_callback=false;
        CHECK(session.capture(source,identity)==InkCloneStatus::WrongContext);
        backend.in_callback=true;
        backend.source_ok=false;
        CHECK(session.capture(source,identity)==InkCloneStatus::InvalidSource);
        backend.source_ok=true;
        CHECK(backend.clone_calls==0);

        backend.clone_ok=false;
        CHECK(session.capture(source,identity)==InkCloneStatus::CloneFailed);
        CHECK(!session.active() && backend.release_calls==0);
        backend.clone_ok=true;
        backend.returned_partial=true;
        CHECK(session.capture(source,identity)==InkCloneStatus::CloneFailed);
        CHECK(!session.active() && backend.release_calls==1);
        backend.returned_partial=false;

        backend.clone_same_source=true;
        CHECK(session.capture(source,identity)==InkCloneStatus::CloneFailed);
        CHECK(!session.active() && backend.release_calls==2);
        backend.clone_same_source=false;
        backend.clone_same_ref=true;
        CHECK(session.capture(source,identity)==InkCloneStatus::CloneFailed);
        CHECK(!session.active() && backend.release_calls==3);
        backend.clone_same_ref=false;

        backend.verify_ok=false;
        CHECK(session.capture(source,identity)==InkCloneStatus::VerificationFailed);
        CHECK(!session.active() && backend.release_calls==4);
        backend.verify_ok=true;

        CHECK(session.capture(source,identity)==InkCloneStatus::Owned);
        CHECK(session.active());
        CHECK(session.source_identity().path_hash==identity.path_hash);
        CHECK(session.capture(source,identity)==InkCloneStatus::AlreadyOwned);
        CHECK(backend.clone_calls==6);
        CHECK(backend.release_calls==4);
        // A retained independent engine clone must outlive the borrowed
        // input handle. The source view is not referenced on reset.
        session.reset();
        CHECK(!session.active() && session.source_identity().path_hash==0);
        CHECK(backend.release_calls==5);
        session.reset();
        CHECK(backend.release_calls==5);
        CHECK(session.capture(source,identity)==InkCloneStatus::Owned);
    }
    // Destructor releases the owned clone exactly once.
    CHECK(backend.release_calls==6);
    CHECK(backend.created==backend.disposed);
    std::cout<<"Engine-owned Ink clone contract handles all failure paths\n";
}
