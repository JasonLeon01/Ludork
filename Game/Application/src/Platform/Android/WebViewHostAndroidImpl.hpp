#pragma once

#include <Runtime/WebViewHost.hpp>
#include <android/native_activity.h>
#include <jni.h>

#include <mutex>

namespace ludork::application {

class WebViewHostAndroidImpl final : public ludork::runtime::webview::Host {
public:
    explicit WebViewHostAndroidImpl(ANativeActivity* activity);
    ~WebViewHostAndroidImpl() override;
    bool open(std::uint64_t id, const std::string& url) override;
    void close(std::uint64_t id) override;

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

    std::mutex mutex_;
    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    std::uint64_t session_ = 0;
};

}  // namespace ludork::application
