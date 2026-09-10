#include "DesktopTextInputHost.hpp"

#if !defined(__APPLE__)
#include "DesktopTextInputHostImpl.hpp"
#include "NativeInputMethod.hpp"
#include <SFML/System/String.hpp>
#include <utility>
#include <algorithm>
#if defined(_WIN32)
#include <imm.h>
#include <vector>
#endif

namespace ludork::global {

DesktopTextInputHostImpl::DesktopTextInputHostImpl(sf::WindowBase& window)
    : window_(window) {}
DesktopTextInputHostImpl::~DesktopTextInputHostImpl() {
    end(id_);
}
bool DesktopTextInputHostImpl::isModal() const {
    return false;
}
bool DesktopTextInputHostImpl::handlesKeyboard() const {
    return false;
}

bool DesktopTextInputHostImpl::begin(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::Request& request, Sink sink) {
    id_ = id;
    sink_ = std::move(sink);
    setNativeInputMethodDisabled(window_.getNativeHandle(), false);
    window_.setKeyRepeatEnabled(true);
#if defined(_WIN32)
    nativeWindow_ = window_.getNativeHandle();
    SetPropW(nativeWindow_, L"LudorkTextInputHost", this);
    previousProcedure_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(nativeWindow_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(
                              &DesktopTextInputHostImpl::windowProcedure)));
#endif
    update(id, request.state, request.caretRect);
    return true;
}

void DesktopTextInputHostImpl::update(ludork::engine::text_input::SessionId id,
                                      const ludork::engine::text_input::State&,
                                      const sf::FloatRect& caretRect) {
    if (id == 0 || id != id_) {
        return;
    }
#if defined(_WIN32)
    if (HIMC context = ImmGetContext(nativeWindow_)) {
        COMPOSITIONFORM composition{};
        composition.dwStyle = CFS_POINT;
        composition.ptCurrentPos = {
            static_cast<LONG>(caretRect.position.x),
            static_cast<LONG>(caretRect.position.y + caretRect.size.y)};
        ImmSetCompositionWindow(context, &composition);
        CANDIDATEFORM candidates{};
        candidates.dwStyle = CFS_EXCLUDE;
        candidates.ptCurrentPos = composition.ptCurrentPos;
        candidates.rcArea = {
            static_cast<LONG>(caretRect.position.x),
            static_cast<LONG>(caretRect.position.y),
            static_cast<LONG>(caretRect.position.x + caretRect.size.x),
            static_cast<LONG>(caretRect.position.y + caretRect.size.y)};
        ImmSetCandidateWindow(context, &candidates);
        ImmReleaseContext(nativeWindow_, context);
    }
#else
    static_cast<void>(caretRect);
#endif
}

void DesktopTextInputHostImpl::end(ludork::engine::text_input::SessionId id) {
    if (id == 0 || id != id_) {
        return;
    }
    id_ = 0;
#if defined(_WIN32)
    if (IsWindow(nativeWindow_)) {
        if (HIMC context = ImmGetContext(nativeWindow_)) {
            ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmReleaseContext(nativeWindow_, context);
        }
        SetWindowLongPtrW(nativeWindow_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(previousProcedure_));
        RemovePropW(nativeWindow_, L"LudorkTextInputHost");
    }
    nativeWindow_ = nullptr;
    previousProcedure_ = nullptr;
#endif
    sink_ = {};
    setNativeInputMethodDisabled(window_.getNativeHandle(), true);
}

#if defined(_WIN32)
LRESULT CALLBACK DesktopTextInputHostImpl::windowProcedure(HWND window,
                                                           UINT message,
                                                           WPARAM word,
                                                           LPARAM parameter) {
    auto* host = static_cast<DesktopTextInputHostImpl*>(
        GetPropW(window, L"LudorkTextInputHost"));
    if (host == nullptr) {
        return DefWindowProcW(window, message, word, parameter);
    }
    if ((message == WM_IME_COMPOSITION || message == WM_IME_ENDCOMPOSITION) &&
        host->sink_) {
        ludork::engine::text_input::Event event;
        event.kind = ludork::engine::text_input::Event::Kind::Preedit;
        if (message == WM_IME_COMPOSITION && (parameter & GCS_COMPSTR) != 0) {
            if (HIMC context = ImmGetContext(window)) {
                const LONG count =
                    ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
                if (count > 0) {
                    std::vector<char16_t> characters(
                        static_cast<std::size_t>(count) / sizeof(char16_t));
                    ImmGetCompositionStringW(context, GCS_COMPSTR,
                                             characters.data(), count);
                    const sf::U8String bytes =
                        sf::String::fromUtf16(characters.begin(),
                                              characters.end())
                            .toUtf8();
                    event.state.preedit.assign(
                        reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
                    const LONG cursor = ImmGetCompositionStringW(
                        context, GCS_CURSORPOS, nullptr, 0);
                    const std::size_t position = std::min(
                        characters.size(),
                        static_cast<std::size_t>(std::max(0L, cursor)));
                    event.state.preeditCaret =
                        sf::String::fromUtf16(characters.begin(),
                                              characters.begin() + position)
                            .toUtf8()
                            .size();
                }
                ImmReleaseContext(window, context);
            }
        }
        host->sink_(host->id_, std::move(event));
    }
    return CallWindowProcW(host->previousProcedure_, window, message, word,
                           parameter);
}
#endif

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createDesktopTextInputHost(sf::WindowBase& window) {
    return std::make_shared<DesktopTextInputHostImpl>(window);
}

}  // namespace ludork::global
#endif
