#pragma once
#include <cstdint>

namespace dreamui {

// Symbol published in the MIT-licensed Codeware address map. Resolution
// must use DreamUI's own build-fingerprinted address database, never a
// runtime call into Codeware or RED4ext.
inline constexpr std::uint32_t kNativeISerializableCloneHash=2601784524u;

// Engine Handle<ISerializable> is a pair {instance, refCount}. This is a
// BORROWED view of a strong handle from serializer->serializables; copying
// these 16 bytes DOES NOT acquire a new reference. The view is valid only
// while the game's strong serializer handle is live in the callback.
struct InkBorrowedHandle {
    std::uint64_t instance=0;
    std::uint64_t ref_count=0;
};
static_assert(sizeof(InkBorrowedHandle)==0x10);

// A clone's native strong reference is independently owned and must be
// released using native Handle semantics (including destructor/weak-ref
// bookkeeping). NEVER free the two fields with free/delete/VirtualFree.
struct InkOwnedNativeHandle {
    std::uint64_t instance=0;
    std::uint64_t ref_count=0;
    bool active() const noexcept {return instance!=0 && ref_count!=0;}
};
static_assert(sizeof(InkOwnedNativeHandle)==0x10);

struct InkCloneIdentity {
    std::uint64_t path_hash=0;
    std::uint64_t source_instance_id=0;
};

// Runtime-specific, engine-thread-aware implementation. It must be built
// independently into DreamUI; source research does not imply Codeware is
// installed. No Windows/backend implementation is wired to the ASI yet.
class NativeInkCloneOps {
public:
    virtual ~NativeInkCloneOps()=default;

    // Returns true only in a game-owned callback with a live, owning
    // serializer Handle<ISerializable>; cannot be faked by the Tab worker.
    virtual bool in_owning_callback() const noexcept=0;

    // Verify RTTI == inkStyleResource, path and nonzero instance identity,
    // and confirm the incoming handle has a current positive strong count.
    virtual bool verify_source(InkBorrowedHandle source,
                               InkCloneIdentity expected) noexcept=0;

    // Invoke the verified native ISerializable::Clone ABI through the game's
    // own address database. On failure, 'out' may contain a partial handle
    // and MUST be releasable through release_owned().
    virtual bool clone_owned(InkBorrowedHandle source,
                             InkOwnedNativeHandle& out) noexcept=0;

    // Verify clone RTTI/path and that the cloned styles array is nonempty,
    // structurally bounded, and independent of the source allocation.
    // The clone may have a DIFFERENT serialization instance ID.
    virtual bool verify_clone(const InkOwnedNativeHandle& clone,
                              InkCloneIdentity source_identity) noexcept=0;

    // Must be safe to invoke from the owner destructor: dispatch final
    // native Handle release to an appropriate engine thread if needed,
    // and guarantee eventually consuming the handle. It MUST NOT use
    // free/delete or a manually invented refcount destructor.
    virtual void release_owned(InkOwnedNativeHandle& handle) noexcept=0;
};

enum class InkCloneStatus {
    Unused,
    InvalidSource,
    WrongContext,
    AlreadyOwned,
    CloneFailed,
    VerificationFailed,
    Owned
};

// Owns ONE game-created clone. Never retains the BORROWED input handle.
// The backend outlives this session. This class only orchestrates the
// native handle lifecycle; it cannot rehydrate resource.styles or call
// PostLoad/RefreshResource. Failures release any partially created clone.
class InkOwnedCloneSession {
public:
    explicit InkOwnedCloneSession(NativeInkCloneOps& ops) noexcept:ops_(ops){}
    ~InkOwnedCloneSession(){reset();}
    InkOwnedCloneSession(const InkOwnedCloneSession&)=delete;
    InkOwnedCloneSession& operator=(const InkOwnedCloneSession&)=delete;
    InkOwnedCloneSession(InkOwnedCloneSession&&)=delete;
    InkOwnedCloneSession& operator=(InkOwnedCloneSession&&)=delete;

    InkCloneStatus capture(InkBorrowedHandle source,
                           InkCloneIdentity identity) noexcept;
    void reset() noexcept;
    bool active() const noexcept {return owned_.active();}
    InkCloneIdentity source_identity() const noexcept {return identity_;}
private:
    NativeInkCloneOps& ops_;
    InkOwnedNativeHandle owned_{};
    InkCloneIdentity identity_{};
};

} // namespace dreamui
