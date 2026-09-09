#pragma once

#include "TextInputHostHarmony.hpp"

#include <Input/TextInputHost.hpp>

#include <memory>
#include <mutex>

namespace ludork::application {

class TextInputHostHarmonyImpl final
    : public ludork::engine::text_input::TextInputHost,
      public std::enable_shared_from_this<TextInputHostHarmonyImpl> {
public:
    struct Payload {
        std::weak_ptr<TextInputHostHarmonyImpl> owner;
        ludork::engine::text_input::SessionId id = 0;
        bool visible = false;
        ludork::engine::text_input::Request request;
    };

    TextInputHostHarmonyImpl(napi_env env, napi_threadsafe_function function);
    ~TextInputHostHarmonyImpl() override;
    bool isModal() const override;
    bool handlesKeyboard() const override;
    bool begin(ludork::engine::text_input::SessionId id,
               const ludork::engine::text_input::Request& request,
               Sink sink) override;
    void update(ludork::engine::text_input::SessionId id,
                const ludork::engine::text_input::State& state,
                const sf::FloatRect& caretRect) override;
    void end(ludork::engine::text_input::SessionId id) override;
    void complete(ludork::engine::text_input::SessionId id, bool accepted,
                  const std::string& text);
    void close();
    void configure(napi_env env, napi_threadsafe_function function);
    bool owns(napi_env env) const;
    bool accepts(const Payload& payload) const;

private:
    napi_env env_;
    napi_threadsafe_function function_;
    mutable std::mutex mutex_;
    ludork::engine::text_input::SessionId session_ = 0;
    Sink sink_;
};

}  // namespace ludork::application
