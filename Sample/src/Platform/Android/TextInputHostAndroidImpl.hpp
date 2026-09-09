#pragma once

#include "TextInputHostAndroid.hpp"

#include <android/native_activity.h>
#include <jni.h>

#include <mutex>

namespace ludork::application {

class TextInputHostAndroidImpl final
    : public ludork::engine::text_input::TextInputHost {
public:
    explicit TextInputHostAndroidImpl(ANativeActivity* activity);
    ~TextInputHostAndroidImpl() override;
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

private:
    class JniEnvironment {
    public:
        explicit JniEnvironment(JavaVM* vm);
        ~JniEnvironment();
        JNIEnv* get() const;

    private:
        JavaVM* vm_;
        JNIEnv* environment_ = nullptr;
        bool attached_ = false;
    };

    ANativeActivity* activity_;
    std::mutex mutex_;
    ludork::engine::text_input::SessionId session_ = 0;
    Sink sink_;
};

}  // namespace ludork::application
