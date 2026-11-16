#pragma once

#include "WebTextInputHost.hpp"

#include <string>

namespace ludork::global {

class WebTextInputHostImpl final
    : public ludork::engine::text_input::TextInputHost {
public:
    explicit WebTextInputHostImpl(sf::WindowHandle window);
    ~WebTextInputHostImpl() override;
    bool isAvailable() const override;
    bool isModal() const override;
    bool handlesKeyboard() const override;
    bool begin(ludork::engine::text_input::SessionId id,
               const ludork::engine::text_input::Request& request,
               Sink sink) override;
    void update(ludork::engine::text_input::SessionId id,
                const ludork::engine::text_input::State& state,
                const sf::FloatRect& caretRect) override;
    void end(ludork::engine::text_input::SessionId id) override;

private:
    std::string canvas_;
    ludork::engine::text_input::SessionId id_ = 0;
    Sink sink_;
};

}  // namespace ludork::global
