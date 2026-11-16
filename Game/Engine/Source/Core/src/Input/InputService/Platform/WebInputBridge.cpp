#include "WebInputBridge.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

namespace ludork::engine::platform_input {
namespace {

// clang-format off
EM_JS(void, attachTouchCancellation, (const char* selector), {
    var canvas = document.querySelector(UTF8ToString(selector));
    var previous = Module['ludorkTouchCancellation'];
    if (previous?.canvas === canvas) return;
    previous?.dispose();
    if (!canvas) return;
    var state = { canvas, active: new Set(), ends: [], dispose: null };
    var start = event => {
        for (var touch of event.changedTouches) {
            if (state.active.size < 32) state.active.add(touch.identifier);
        }
    };
    var end = event => {
        for (var touch of event.changedTouches) {
            if (state.active.delete(touch.identifier)) state.ends.push(event.type === 'touchcancel');
        }
    };
    var cancel = () => {
        for (var ignored of state.active) state.ends.push(true);
        state.active.clear();
    };
    var hidden = () => { if (document.hidden) cancel(); };
    canvas.addEventListener('touchstart', start, true);
    canvas.addEventListener('touchend', end, true);
    canvas.addEventListener('touchcancel', end, true);
    window.addEventListener('blur', cancel, true);
    document.addEventListener('visibilitychange', hidden, true);
    state.dispose = () => {
        canvas.removeEventListener('touchstart', start, true);
        canvas.removeEventListener('touchend', end, true);
        canvas.removeEventListener('touchcancel', end, true);
        window.removeEventListener('blur', cancel, true);
        document.removeEventListener('visibilitychange', hidden, true);
    };
    Module['ludorkTouchCancellation'] = state;
});

EM_JS(void, detachTouchCancellation, (), {
    Module['ludorkTouchCancellation']?.dispose();
    delete Module['ludorkTouchCancellation'];
});

EM_JS(bool, consumeTouchCancellation, (), {
    return Module['ludorkTouchCancellation']?.ends.shift() === true;
});
// clang-format on

}  // namespace

void attachWebTouchCancellation(sf::WindowBase& window) {
    attachTouchCancellation(window.getNativeHandle());
}

void detachWebTouchCancellation() noexcept {
    detachTouchCancellation();
}

bool consumeWebTouchCancellation() {
    return consumeTouchCancellation();
}

}  // namespace ludork::engine::platform_input
#endif
