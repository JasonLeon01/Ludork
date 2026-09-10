#pragma once

#include <Input/TextInputHost.hpp>
#include <cstdint>
#include <string_view>

namespace ludork::global {

class EmbeddedTextInputHostImpl final
    : public ludork::engine::text_input::TextInputHost {
public:
    ~EmbeddedTextInputHostImpl() override;
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
    bool send(std::string_view action, const sf::FloatRect* caretRect);

    ludork::engine::text_input::SessionId id_ = 0;
    std::uint64_t connectionId_ = 0;
    sf::FloatRect caretRect_;
};

}  // namespace ludork::global
