#pragma once

#include <Runtime/WebViewHost.hpp>
#include <napi/native_api.h>

#include <memory>
#include <mutex>

namespace ludork::application {

class WebViewHostHarmonyImpl final
    : public ludork::runtime::webview::Host,
      public std::enable_shared_from_this<WebViewHostHarmonyImpl> {
public:
    struct Payload {
        std::weak_ptr<WebViewHostHarmonyImpl> owner;
        std::uint64_t id = 0;
        std::uint64_t revision = 0;
        bool visible = false;
        std::string url;
    };

    WebViewHostHarmonyImpl(napi_env env, napi_threadsafe_function function);
    ~WebViewHostHarmonyImpl() override;
    bool open(std::uint64_t id, const std::string& url) override;
    void close(std::uint64_t id) override;
    void complete(std::uint64_t id);
    void release();
    void configure(napi_env env, napi_threadsafe_function function);
    bool owns(napi_env env) const;
    bool accepts(const Payload& payload) const;

private:
    napi_env env_;
    napi_threadsafe_function function_;
    mutable std::mutex mutex_;
    std::uint64_t session_ = 0;
    std::uint64_t revision_ = 0;
};

}  // namespace ludork::application
