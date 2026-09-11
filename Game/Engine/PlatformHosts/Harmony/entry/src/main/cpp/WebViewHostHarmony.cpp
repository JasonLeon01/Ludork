#include "WebViewHostHarmony.hpp"
#include "WebViewHostHarmonyImpl.hpp"

#include <array>
#include <charconv>
#include <utility>
#include <vector>

namespace {

std::mutex hostMutex;
std::shared_ptr<ludork::application::WebViewHostHarmonyImpl> currentHost;

bool readString(napi_env env, napi_value value, std::string& text) {
    std::size_t length = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) !=
        napi_ok) {
        return false;
    }
    std::vector<char> bytes(length + 1);
    if (napi_get_value_string_utf8(env, value, bytes.data(), bytes.size(),
                                   &length) != napi_ok) {
        return false;
    }
    text.assign(bytes.data(), length);
    return true;
}

bool writeString(napi_env env, const std::string& text, napi_value* value) {
    return napi_create_string_utf8(env, text.data(), text.size(), value) ==
           napi_ok;
}

void callWebViewRequest(napi_env env, napi_value function, void*, void* data) {
    const std::unique_ptr<ludork::application::WebViewHostHarmonyImpl::Payload>
        payload(
            static_cast<ludork::application::WebViewHostHarmonyImpl::Payload*>(
                data));
    if (env == nullptr || function == nullptr || payload == nullptr) {
        return;
    }
    const std::shared_ptr<ludork::application::WebViewHostHarmonyImpl> host =
        payload->owner.lock();
    if (host == nullptr || !host->accepts(*payload)) {
        return;
    }
    std::array<napi_value, 3> arguments{};
    napi_value receiver{};
    napi_value ignored{};
    if (!writeString(env, std::to_string(payload->id), &arguments[0]) ||
        napi_get_boolean(env, payload->visible, &arguments[1]) != napi_ok ||
        !writeString(env, payload->url, &arguments[2]) ||
        napi_get_global(env, &receiver) != napi_ok ||
        napi_call_function(env, receiver, function, arguments.size(),
                           arguments.data(), &ignored) != napi_ok) {
        host->complete(payload->id);
    }
}

void releaseHost(napi_env env) {
    std::shared_ptr<ludork::application::WebViewHostHarmonyImpl> host;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        host = currentHost;
    }
    if (host != nullptr && host->owns(env)) {
        host->release();
    }
}

void cleanupHost(void* environment) {
    releaseHost(static_cast<napi_env>(environment));
}

napi_value configureWebViewHost(napi_env env, napi_callback_info info) {
    napi_value callback{};
    std::size_t count = 1;
    napi_valuetype type{};
    if (napi_get_cb_info(env, info, &count, &callback, nullptr, nullptr) !=
            napi_ok ||
        count != 1 || napi_typeof(env, callback, &type) != napi_ok ||
        type != napi_function) {
        napi_throw_type_error(env, nullptr,
                              "configureWebViewHost requires a function");
        return nullptr;
    }
    napi_value resourceName{};
    napi_threadsafe_function function{};
    if (!writeString(env, "LudorkWebView", &resourceName) ||
        napi_create_threadsafe_function(
            env, callback, nullptr, resourceName, 0, 1, nullptr, nullptr,
            nullptr, callWebViewRequest, &function) != napi_ok) {
        napi_throw_error(env, nullptr, "Unable to create the WebView callback");
        return nullptr;
    }
    std::shared_ptr<ludork::application::WebViewHostHarmonyImpl> host;
    bool first = false;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        first = currentHost == nullptr;
        if (first) {
            currentHost =
                std::make_shared<ludork::application::WebViewHostHarmonyImpl>(
                    env, function);
        }
        host = currentHost;
    }
    if (first) {
        ludork::runtime::webview::setHostFactory([](sf::WindowHandle) {
            const std::lock_guard<std::mutex> lock(hostMutex);
            return currentHost;
        });
    } else {
        host->configure(env, function);
    }
    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

napi_value clearWebViewHost(napi_env env, napi_callback_info) {
    releaseHost(env);
    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

napi_value completeWebView(napi_env env, napi_callback_info info) {
    napi_value argument{};
    std::size_t count = 1;
    std::string idText;
    if (napi_get_cb_info(env, info, &count, &argument, nullptr, nullptr) !=
            napi_ok ||
        count != 1 || !readString(env, argument, idText)) {
        napi_throw_type_error(env, nullptr,
                              "completeWebView requires a session ID");
        return nullptr;
    }
    std::uint64_t id = 0;
    const std::from_chars_result parsed =
        std::from_chars(idText.data(), idText.data() + idText.size(), id);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != idText.data() + idText.size()) {
        napi_throw_type_error(env, nullptr, "Invalid WebView session ID");
        return nullptr;
    }
    std::shared_ptr<ludork::application::WebViewHostHarmonyImpl> host;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        host = currentHost;
    }
    if (host != nullptr && host->owns(env)) {
        host->complete(id);
    }
    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

}  // namespace

namespace ludork::application {

WebViewHostHarmonyImpl::WebViewHostHarmonyImpl(
    napi_env env, napi_threadsafe_function function)
    : env_(env), function_(function) {}

WebViewHostHarmonyImpl::~WebViewHostHarmonyImpl() {
    release();
}

bool WebViewHostHarmonyImpl::open(std::uint64_t id, const std::string& url) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (function_ == nullptr) {
        return false;
    }
    std::unique_ptr<Payload> payload = std::make_unique<Payload>();
    payload->owner = weak_from_this();
    payload->id = id;
    payload->revision = revision_ + 1;
    payload->visible = true;
    payload->url = url;
    if (napi_call_threadsafe_function(function_, payload.get(),
                                      napi_tsfn_nonblocking) != napi_ok) {
        return false;
    }
    revision_ = payload->revision;
    session_ = id;
    static_cast<void>(payload.release());
    return true;
}

void WebViewHostHarmonyImpl::close(std::uint64_t id) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (session_ != id || session_ == 0) {
        return;
    }
    session_ = 0;
    ++revision_;
    if (function_ != nullptr) {
        std::unique_ptr<Payload> payload = std::make_unique<Payload>();
        payload->owner = weak_from_this();
        payload->id = id;
        payload->revision = revision_;
        if (napi_call_threadsafe_function(function_, payload.get(),
                                          napi_tsfn_nonblocking) == napi_ok) {
            static_cast<void>(payload.release());
        }
    }
}

void WebViewHostHarmonyImpl::complete(std::uint64_t id) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (session_ != id || session_ == 0) {
            return;
        }
        session_ = 0;
        ++revision_;
    }
    ludork::runtime::webview::notifyClosed(id);
}

void WebViewHostHarmonyImpl::release() {
    napi_threadsafe_function previous{};
    std::uint64_t id = 0;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        previous = std::exchange(function_, nullptr);
        id = std::exchange(session_, 0);
        ++revision_;
    }
    if (previous != nullptr) {
        static_cast<void>(
            napi_release_threadsafe_function(previous, napi_tsfn_abort));
    }
    if (id != 0) {
        ludork::runtime::webview::notifyClosed(id);
    }
}

void WebViewHostHarmonyImpl::configure(napi_env env,
                                       napi_threadsafe_function function) {
    release();
    const std::lock_guard<std::mutex> lock(mutex_);
    env_ = env;
    function_ = function;
}

bool WebViewHostHarmonyImpl::owns(napi_env env) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return env_ == env;
}

bool WebViewHostHarmonyImpl::accepts(const Payload& payload) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return function_ != nullptr && revision_ == payload.revision &&
           (!payload.visible || session_ == payload.id);
}

bool registerHarmonyWebViewHost(napi_env env, napi_value exports) {
    const std::array properties{
        napi_property_descriptor{"configureWebViewHost", nullptr,
                                 configureWebViewHost, nullptr, nullptr,
                                 nullptr, napi_default, nullptr},
        napi_property_descriptor{"clearWebViewHost", nullptr, clearWebViewHost,
                                 nullptr, nullptr, nullptr, napi_default,
                                 nullptr},
        napi_property_descriptor{"completeWebView", nullptr, completeWebView,
                                 nullptr, nullptr, nullptr, napi_default,
                                 nullptr}};
    return napi_define_properties(env, exports, properties.size(),
                                  properties.data()) == napi_ok &&
           napi_add_env_cleanup_hook(env, cleanupHost, env) == napi_ok;
}

}  // namespace ludork::application
