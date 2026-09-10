#include "TextInputHostHarmonyImpl.hpp"

#include <Input/TextInputService.hpp>
#include <LudorkPlatform.hpp>

#include <array>
#include <charconv>
#include <utility>
#include <vector>

namespace {

std::mutex hostMutex;
std::shared_ptr<ludork::application::TextInputHostHarmonyImpl> currentHost;

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

void callTextInputRequest(napi_env env, napi_value function, void*,
                          void* data) {
    const std::unique_ptr<
        ludork::application::TextInputHostHarmonyImpl::Payload>
        payload(static_cast<
                ludork::application::TextInputHostHarmonyImpl::Payload*>(data));
    if (env == nullptr || function == nullptr || payload == nullptr) {
        return;
    }
    const std::shared_ptr<ludork::application::TextInputHostHarmonyImpl> host =
        payload->owner.lock();
    if (host == nullptr || !host->accepts(*payload)) {
        return;
    }
    std::array<napi_value, 8> arguments{};
    if (!writeString(env, std::to_string(payload->id), &arguments[0]) ||
        napi_get_boolean(env, payload->visible, &arguments[1]) != napi_ok ||
        !writeString(env, payload->request.state.text, &arguments[2]) ||
        !writeString(env, payload->request.title, &arguments[3]) ||
        !writeString(env, payload->request.prompt, &arguments[4]) ||
        !writeString(env, payload->request.placeholder, &arguments[5]) ||
        !writeString(env, payload->request.confirmText, &arguments[6]) ||
        !writeString(env, payload->request.cancelText, &arguments[7])) {
        host->complete(payload->id, false, {});
        return;
    }
    napi_value receiver{};
    napi_value ignored{};
    if (napi_get_global(env, &receiver) != napi_ok ||
        napi_call_function(env, receiver, function, arguments.size(),
                           arguments.data(), &ignored) != napi_ok) {
        host->complete(payload->id, false, {});
    }
}

void releaseHost(napi_env env) {
    std::shared_ptr<ludork::application::TextInputHostHarmonyImpl> host;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        if (currentHost == nullptr || !currentHost->owns(env)) {
            return;
        }
        host = currentHost;
    }
    host->close();
}

void cleanupHost(void* environment) {
    releaseHost(static_cast<napi_env>(environment));
}

napi_value configureTextInputHost(napi_env env, napi_callback_info info) {
#if defined(LUDORK_MOBILE)
    napi_value callback{};
    std::size_t count = 1;
    napi_valuetype type{};
    if (napi_get_cb_info(env, info, &count, &callback, nullptr, nullptr) !=
            napi_ok ||
        count != 1 || napi_typeof(env, callback, &type) != napi_ok ||
        type != napi_function) {
        napi_throw_type_error(env, nullptr,
                              "configureTextInputHost requires a function");
        return nullptr;
    }
    napi_value resourceName{};
    napi_threadsafe_function function{};
    if (!writeString(env, "LudorkTextInput", &resourceName) ||
        napi_create_threadsafe_function(
            env, callback, nullptr, resourceName, 0, 1, nullptr, nullptr,
            nullptr, callTextInputRequest, &function) != napi_ok) {
        napi_throw_error(env, nullptr,
                         "Unable to create the text input callback");
        return nullptr;
    }
    std::shared_ptr<ludork::application::TextInputHostHarmonyImpl> host;
    bool first = false;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        first = currentHost == nullptr;
        if (first) {
            currentHost =
                std::make_shared<ludork::application::TextInputHostHarmonyImpl>(
                    env, function);
        }
        host = currentHost;
    }
    if (first) {
        ludork::engine::text_input::service().setHost(host);
    } else {
        host->configure(env, function);
    }
#else
    static_cast<void>(info);
#endif
    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

napi_value clearTextInputHost(napi_env env, napi_callback_info) {
    releaseHost(env);
    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

napi_value completeTextInput(napi_env env, napi_callback_info info) {
    std::array<napi_value, 3> arguments{};
    std::size_t count = arguments.size();
    std::string idText;
    std::string text;
    bool accepted = false;
    if (napi_get_cb_info(env, info, &count, arguments.data(), nullptr,
                         nullptr) != napi_ok ||
        count != arguments.size() || !readString(env, arguments[0], idText) ||
        napi_get_value_bool(env, arguments[1], &accepted) != napi_ok ||
        !readString(env, arguments[2], text)) {
        napi_throw_type_error(
            env, nullptr,
            "completeTextInput requires session ID, acceptance and text");
        return nullptr;
    }
    ludork::engine::text_input::SessionId id = 0;
    const std::from_chars_result parsed =
        std::from_chars(idText.data(), idText.data() + idText.size(), id);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != idText.data() + idText.size()) {
        napi_throw_type_error(env, nullptr, "Invalid text input session ID");
        return nullptr;
    }
    std::shared_ptr<ludork::application::TextInputHostHarmonyImpl> host;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        host = currentHost;
    }
    if (host != nullptr && host->owns(env)) {
        host->complete(id, accepted, text);
    }
    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

}  // namespace

namespace ludork::application {

TextInputHostHarmonyImpl::TextInputHostHarmonyImpl(
    napi_env env, napi_threadsafe_function function)
    : env_(env), function_(function) {}

TextInputHostHarmonyImpl::~TextInputHostHarmonyImpl() {
    close();
}

bool TextInputHostHarmonyImpl::isModal() const {
    return true;
}

bool TextInputHostHarmonyImpl::handlesKeyboard() const {
    return true;
}

bool TextInputHostHarmonyImpl::begin(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::Request& request, Sink sink) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (function_ == nullptr) {
        return false;
    }
    session_ = id;
    sink_ = std::move(sink);
    std::unique_ptr<Payload> payload = std::make_unique<Payload>();
    payload->owner = weak_from_this();
    payload->id = id;
    payload->visible = true;
    payload->request = request;
    if (napi_call_threadsafe_function(function_, payload.get(),
                                      napi_tsfn_nonblocking) != napi_ok) {
        session_ = 0;
        sink_ = {};
        return false;
    }
    static_cast<void>(payload.release());
    return true;
}

void TextInputHostHarmonyImpl::update(ludork::engine::text_input::SessionId,
                                      const ludork::engine::text_input::State&,
                                      const sf::FloatRect&) {}

void TextInputHostHarmonyImpl::end(ludork::engine::text_input::SessionId id) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (session_ != id || session_ == 0) {
        return;
    }
    session_ = 0;
    sink_ = {};
    if (function_ != nullptr) {
        std::unique_ptr<Payload> payload = std::make_unique<Payload>();
        payload->owner = weak_from_this();
        payload->id = id;
        if (napi_call_threadsafe_function(function_, payload.get(),
                                          napi_tsfn_nonblocking) == napi_ok) {
            static_cast<void>(payload.release());
        }
    }
}

void TextInputHostHarmonyImpl::complete(
    ludork::engine::text_input::SessionId id, bool accepted,
    const std::string& text) {
    Sink callback;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (session_ != id || session_ == 0) {
            return;
        }
        session_ = 0;
        callback = std::exchange(sink_, {});
    }
    ludork::engine::text_input::Event event;
    event.kind = ludork::engine::text_input::Event::Kind::Complete;
    event.accepted = accepted;
    event.state.text = text;
    callback(id, std::move(event));
}

void TextInputHostHarmonyImpl::close() {
    napi_threadsafe_function previous{};
    Sink callback;
    ludork::engine::text_input::SessionId id = 0;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        previous = std::exchange(function_, nullptr);
        callback = std::exchange(sink_, {});
        id = std::exchange(session_, 0);
    }
    if (previous != nullptr) {
        static_cast<void>(
            napi_release_threadsafe_function(previous, napi_tsfn_abort));
    }
    if (callback) {
        ludork::engine::text_input::Event event;
        event.kind = ludork::engine::text_input::Event::Kind::Complete;
        event.accepted = false;
        callback(id, std::move(event));
    }
}

bool TextInputHostHarmonyImpl::owns(napi_env env) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return env_ == env;
}

void TextInputHostHarmonyImpl::configure(napi_env env,
                                         napi_threadsafe_function function) {
    close();
    const std::lock_guard<std::mutex> lock(mutex_);
    env_ = env;
    function_ = function;
}

bool TextInputHostHarmonyImpl::accepts(const Payload& payload) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return function_ != nullptr && (!payload.visible || session_ == payload.id);
}

bool registerHarmonyTextInputHost(napi_env env, napi_value exports) {
    const std::array properties{
        napi_property_descriptor{"configureTextInputHost", nullptr,
                                 configureTextInputHost, nullptr, nullptr,
                                 nullptr, napi_default, nullptr},
        napi_property_descriptor{"clearTextInputHost", nullptr,
                                 clearTextInputHost, nullptr, nullptr, nullptr,
                                 napi_default, nullptr},
        napi_property_descriptor{"completeTextInput", nullptr,
                                 completeTextInput, nullptr, nullptr, nullptr,
                                 napi_default, nullptr}};
    return napi_define_properties(env, exports, properties.size(),
                                  properties.data()) == napi_ok &&
           napi_add_env_cleanup_hook(env, cleanupHost, env) == napi_ok;
}

}  // namespace ludork::application
