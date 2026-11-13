#include "dreamui/native_ink.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
using namespace dreamui;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #condition << '\n'; \
    return 1; }} while(false)

struct MockInk final : InkEngineAccess {
    bool engine_thread=true, read_ok=true;
    int write_failures=0, refresh_failures=0;
    unsigned reads=0, writes=0, refreshes=0;
    ResourceIdentity expected{45, 2};
    HdrColor original{2.4f, 0.45f, 7.25f, 0.375f};
    HdrColor current=original;

    bool on_engine_thread() const noexcept override { return engine_thread; }
    bool read_hdr_color(ResourceIdentity id, std::string_view property,
                        HdrColor& color) noexcept override {
        ++reads;
        if (!read_ok || id != expected ||
            property != NativeInkColorSession::kFirstProperty) return false;
        color=current;
        return true;
    }
    bool write_hdr_color(ResourceIdentity id, std::string_view property,
                         HdrColor color) noexcept override {
        ++writes;
        if (id != expected || property != NativeInkColorSession::kFirstProperty) return false;
        // Simulate an engine operation that may modify data BEFORE signaling
        // failure (must be included in rollback journal).
        current=color;
        if (write_failures>0) {
            --write_failures; return false;
        }
        return true;
    }
    bool refresh_resource(ResourceIdentity id) noexcept override {
        ++refreshes;
        if (id != expected) return false;
        if (refresh_failures>0) {
            --refresh_failures;return false;
        }
        return true;
    }
};

int main() {
    static_assert(sizeof(HdrColor)==16 && alignof(HdrColor)==16);
    Theme theme;
    theme.id="dreamui.default";
    theme.colors["text.primary"]={0xEA,0xF0,0xF2};
    std::string error;
    constexpr ResourceIdentity first{45,2};
    constexpr ResourceIdentity next{45,3};
    const InkResourceEvent loaded{first,NativeInkColorSession::kStylePath,InkResourceEventKind::Loaded,true};
    const InkResourceEvent ready{first,NativeInkColorSession::kStylePath,InkResourceEventKind::Ready,true};
    const InkResourceEvent unloaded{first,NativeInkColorSession::kStylePath,InkResourceEventKind::Unloaded,false};

    CHECK(finite_hdr(HdrColor{10.f,0.1f,7.f,1.0f}));
    CHECK(!finite_hdr(HdrColor{std::numeric_limits<float>::quiet_NaN(),0.f,0.f,1.f}));
    auto converted=preview_sdr_color(Rgb{255,0,127},0.375f);
    CHECK(converted.red==1.0f && converted.green==0.f &&
          converted.blue>0.49f && converted.blue<0.51f &&
          converted.alpha==0.375f);

    NativeInkColorSession session;
    MockInk engine;
    CHECK(!session.apply_first_color(theme,engine,error) && !session.ready());
    session.on_resource_event({first,"some/other.inkstyle",InkResourceEventKind::Ready,true});
    CHECK(!session.ready());
    session.on_resource_event({first,NativeInkColorSession::kStylePath,InkResourceEventKind::Ready,false});
    CHECK(!session.ready());
    session.on_resource_event(loaded);
    CHECK(!session.ready());
    session.on_resource_event(ready);
    CHECK(session.ready());

    engine.engine_thread=false;
    CHECK(!session.apply_first_color(theme,engine,error) && engine.reads==0);
    engine.engine_thread=true;
    Theme missing;
    CHECK(!session.apply_first_color(missing,engine,error) && engine.reads==0);
    engine.read_ok=false;
    CHECK(!session.apply_first_color(theme,engine,error) && !session.active());
    engine.read_ok=true;

    CHECK(session.apply_first_color(theme,engine,error));
    CHECK(session.active() && engine.writes==1 && engine.refreshes==1);
    CHECK(engine.current.red>0.9f && engine.current.red<1.0f);
    CHECK(engine.current.alpha==engine.original.alpha);
    CHECK(!session.apply_first_color(theme,engine,error));
    CHECK(session.restore(engine,error));
    CHECK(!session.active() && engine.current==engine.original);
    CHECK(session.restore(engine,error));

    // Partial write failure must restore the complete original HDR floats.
    engine.write_failures=1;
    CHECK(!session.apply_first_color(theme,engine,error));
    CHECK(!session.active() && engine.current==engine.original);
    CHECK(engine.refreshes>=3);

    // Failed refresh after a successful write triggers restoration+refresh.
    engine.refresh_failures=1;
    CHECK(!session.apply_first_color(theme,engine,error));
    CHECK(!session.active() && engine.current==engine.original);

    // Failure while rolling back must retain the journal for manual retry.
    engine.write_failures=2;
    CHECK(!session.apply_first_color(theme,engine,error));
    CHECK(session.active() && engine.current!=engine.original);
    CHECK(session.restore(engine,error) && !session.active());
    CHECK(engine.current==engine.original);

    // Failed restoration never discards the snapshot.
    CHECK(session.apply_first_color(theme,engine,error));
    engine.refresh_failures=1;
    CHECK(!session.restore(engine,error) && session.active());
    CHECK(session.restore(engine,error) && !session.active());
    CHECK(engine.current==engine.original);

    // Resource invalidated while active: never touch a stale generation.
    CHECK(session.apply_first_color(theme,engine,error));
    const auto writes=engine.writes;
    session.on_resource_event({next,NativeInkColorSession::kStylePath,InkResourceEventKind::Ready,true});
    CHECK(session.active()); // must not replace a dirty generation
    session.on_resource_event(unloaded);
    CHECK(!session.active() && !session.ready());
    CHECK(session.restore(engine,error) && engine.writes==writes);
    session.on_resource_event({next,NativeInkColorSession::kStylePath,InkResourceEventKind::Ready,true});
    CHECK(session.ready());
    CHECK(!session.apply_first_color(theme,engine,error)); // backend rejects unknown generation
    session.on_resource_event({next,NativeInkColorSession::kStylePath,InkResourceEventKind::Unloaded,false});
    CHECK(!session.ready());

    // A verified type with malformed float channels must be refused.
    session.on_resource_event(ready);
    engine.current={std::numeric_limits<float>::infinity(),0.f,0.f,1.f};
    CHECK(!session.apply_first_color(theme,engine,error) && !session.active());

    std::cout << "Typed native Ink event lifecycle, HDR snapshot and rollback tests passed\n";
    return 0;
}
