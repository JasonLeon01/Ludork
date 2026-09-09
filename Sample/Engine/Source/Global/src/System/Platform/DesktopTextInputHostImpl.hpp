#pragma once

#include <Input/TextInputHost.hpp>
#include <SFML/Window/WindowBase.hpp>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ludork::global {

class DesktopTextInputHostImpl final
    : public ludork::engine::text_input::TextInputHost {
public:
    explicit DesktopTextInputHostImpl(sf::WindowBase& window);
    ~DesktopTextInputHostImpl() override;
    bool isModal() const override;
    bool handlesKeyboard() const override;
    bool begin(ludork::engine::text_input::SessionId id,
               const ludork::engine::text_input::Request& request,
               Sink sink) override;
    void update(ludork::engine::text_input::SessionId id,
                const ludork::engine::text_input::State& state,
                const sf::FloatRect& caretRect) override;
    void end(ludork::engine::text_input::SessionId id) override;
#if defined(_WIN32)
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message,
                                            WPARAM word, LPARAM parameter);
#endif

private:
    sf::WindowBase& window_;
    ludork::engine::text_input::SessionId id_ = 0;
    Sink sink_;
#if defined(_WIN32)
    WNDPROC previousProcedure_ = nullptr;
    HWND nativeWindow_ = nullptr;
#endif
};

}  // namespace ludork::global
