#include <Application.hpp>
#include <Input/InputService.hpp>
#include <SFML/Main/MainHarmony.hpp>
#include <SFML/Window/Harmony/NativeAppImpl.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <System/NativeDisplayHost.hpp>

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <napi/native_api.h>
#include <string>
#include <utility>
#include <vector>

namespace {

struct DisplayScalePayload {
    float scale{};
    sf::Vector2u gameSize;
    napi_env owner{};
    std::uint64_t revision{};
};

struct DisplayScaleHostState {
    std::mutex mutex;
    napi_threadsafe_function request{};
    napi_env owner{};
    std::uint64_t revision{};
};

DisplayScaleHostState displayScaleHost;

bool readString(napi_env env, napi_value value, std::string& result) {
    napi_valuetype type{};
    if (napi_typeof(env, value, &type) != napi_ok || type != napi_string) {
        return false;
    }

    std::size_t length = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) !=
        napi_ok) {
        return false;
    }

    std::vector<char> buffer(length + 1);
    std::size_t copied = 0;
    if (napi_get_value_string_utf8(env, value, buffer.data(), buffer.size(),
                                   &copied) != napi_ok) {
        return false;
    }

    result.assign(buffer.data(), copied);
    return true;
}

bool readBoolean(napi_env env, napi_value value, bool& result) {
    napi_valuetype type{};
    return napi_typeof(env, value, &type) == napi_ok && type == napi_boolean &&
           napi_get_value_bool(env, value, &result) == napi_ok;
}

void callDisplayScaleRequest(napi_env env, napi_value function, void*,
                             void* data) {
    const std::unique_ptr<DisplayScalePayload> value(
        static_cast<DisplayScalePayload*>(data));
    if (env == nullptr || function == nullptr || value == nullptr) {
        return;
    }

    const std::lock_guard<std::mutex> lock(displayScaleHost.mutex);
    if (displayScaleHost.owner != value->owner ||
        displayScaleHost.revision != value->revision ||
        displayScaleHost.request == nullptr) {
        return;
    }
    std::array<napi_value, 3> arguments{};
    napi_value receiver{};
    napi_value ignored{};
    if (napi_create_double(env, static_cast<double>(value->scale),
                           &arguments[0]) != napi_ok ||
        napi_create_uint32(env, value->gameSize.x, &arguments[1]) != napi_ok ||
        napi_create_uint32(env, value->gameSize.y, &arguments[2]) != napi_ok ||
        napi_get_global(env, &receiver) != napi_ok) {
        return;
    }
    static_cast<void>(napi_call_function(
        env, receiver, function, arguments.size(), arguments.data(), &ignored));
}

void releaseDisplayScaleHost(napi_env owner) noexcept {
    napi_threadsafe_function function{};
    {
        const std::lock_guard<std::mutex> lock(displayScaleHost.mutex);
        if (displayScaleHost.owner != owner) {
            return;
        }
        ludork::global::native_display_host::clearDisplayScaleHost();
        function = std::exchange(displayScaleHost.request, nullptr);
        displayScaleHost.owner = nullptr;
        ++displayScaleHost.revision;
    }
    if (function != nullptr) {
        static_cast<void>(
            napi_release_threadsafe_function(function, napi_tsfn_abort));
    }
}

void cleanupDisplayScaleHost(void* data) {
    releaseDisplayScaleHost(static_cast<napi_env>(data));
}

napi_value configureRuntimePaths(napi_env env, napi_callback_info info) {
    std::array<napi_value, 2> arguments{};
    std::size_t argumentCount = arguments.size();
    if (napi_get_cb_info(env, info, &argumentCount, arguments.data(), nullptr,
                         nullptr) != napi_ok ||
        argumentCount != arguments.size()) {
        napi_throw_type_error(
            env, nullptr,
            "configureRuntimePaths requires runtimeRoot and userDataRoot");
        return nullptr;
    }

    std::string runtimeRoot;
    std::string userDataRoot;
    if (!readString(env, arguments[0], runtimeRoot) ||
        !readString(env, arguments[1], userDataRoot)) {
        napi_throw_type_error(
            env, nullptr, "configureRuntimePaths requires two string paths");
        return nullptr;
    }

    try {
        ludork::application::configureRuntimePaths(
            std::filesystem::path(runtimeRoot),
            std::filesystem::path(userDataRoot));
    } catch (const std::exception& error) {
        napi_throw_error(env, nullptr, error.what());
        return nullptr;
    }

    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

napi_value configureSystemLocale(napi_env env, napi_callback_info info) {
    std::array<napi_value, 1> arguments{};
    std::size_t argumentCount = arguments.size();
    if (napi_get_cb_info(env, info, &argumentCount, arguments.data(), nullptr,
                         nullptr) != napi_ok ||
        argumentCount != arguments.size()) {
        napi_throw_type_error(env, nullptr,
                              "configureSystemLocale requires a locale");
        return nullptr;
    }

    std::string systemLocale;
    if (!readString(env, arguments[0], systemLocale)) {
        napi_throw_type_error(env, nullptr,
                              "configureSystemLocale requires a locale string");
        return nullptr;
    }

    try {
        ludork::application::configureSystemLocale(systemLocale);
    } catch (const std::exception& error) {
        napi_throw_error(env, nullptr, error.what());
        return nullptr;
    }

    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

napi_value configureDisplayScaleHost(napi_env env, napi_callback_info info) {
    std::array<napi_value, 4> arguments{};
    std::size_t argumentCount = arguments.size();
    if (napi_get_cb_info(env, info, &argumentCount, arguments.data(), nullptr,
                         nullptr) != napi_ok ||
        argumentCount != arguments.size()) {
        napi_throw_type_error(env, nullptr,
                              "configureDisplayScaleHost requires "
                              "configurable, maximum window size and callback");
        return nullptr;
    }

    bool configurable = false;
    std::uint32_t maximumWindowedWidth = 0;
    std::uint32_t maximumWindowedHeight = 0;
    napi_valuetype callbackType{};
    if (!readBoolean(env, arguments[0], configurable) ||
        napi_get_value_uint32(env, arguments[1], &maximumWindowedWidth) !=
            napi_ok ||
        napi_get_value_uint32(env, arguments[2], &maximumWindowedHeight) !=
            napi_ok ||
        napi_typeof(env, arguments[3], &callbackType) != napi_ok ||
        callbackType != napi_function) {
        napi_throw_type_error(env, nullptr,
                              "configureDisplayScaleHost requires a boolean, "
                              "maximum window size and function");
        return nullptr;
    }

    napi_value resourceName{};
    napi_threadsafe_function function{};
    if (napi_create_string_utf8(env, "requestDisplayScale", NAPI_AUTO_LENGTH,
                                &resourceName) != napi_ok ||
        napi_create_threadsafe_function(
            env, arguments[3], nullptr, resourceName, 0, 1, nullptr, nullptr,
            nullptr, callDisplayScaleRequest, &function) != napi_ok) {
        napi_throw_error(env, nullptr,
                         "Failed to create the display scale callback");
        return nullptr;
    }

    napi_threadsafe_function previousFunction{};
    std::uint64_t revision = 0;
    {
        const std::lock_guard<std::mutex> lock(displayScaleHost.mutex);
        previousFunction = std::exchange(displayScaleHost.request, function);
        displayScaleHost.owner = env;
        revision = ++displayScaleHost.revision;
        ludork::global::native_display_host::setDisplayScaleHost(
            configurable, {maximumWindowedWidth, maximumWindowedHeight},
            [env, revision](float scale, sf::Vector2u gameSize) {
                std::unique_ptr<DisplayScalePayload> payload =
                    std::make_unique<DisplayScalePayload>();
                payload->scale = scale;
                payload->gameSize = gameSize;
                payload->owner = env;
                payload->revision = revision;
                const std::lock_guard<std::mutex> lock(displayScaleHost.mutex);
                if (displayScaleHost.owner == env &&
                    displayScaleHost.revision == revision &&
                    displayScaleHost.request != nullptr &&
                    napi_call_threadsafe_function(
                        displayScaleHost.request, payload.get(),
                        napi_tsfn_nonblocking) == napi_ok) {
                    static_cast<void>(payload.release());
                }
            });
    }
    if (previousFunction != nullptr) {
        static_cast<void>(napi_release_threadsafe_function(previousFunction,
                                                           napi_tsfn_abort));
    }

    napi_value result{};
    return napi_get_boolean(env, true, &result) == napi_ok ? result : nullptr;
}

napi_value clearDisplayScaleHost(napi_env env, napi_callback_info) {
    releaseDisplayScaleHost(env);
    napi_value result{};
    return napi_get_undefined(env, &result) == napi_ok ? result : nullptr;
}

napi_value submitKeyEvent(napi_env env, napi_callback_info info) {
    std::array<napi_value, 6> arguments{};
    std::size_t argumentCount = arguments.size();
    if (napi_get_cb_info(env, info, &argumentCount, arguments.data(), nullptr,
                         nullptr) != napi_ok ||
        argumentCount != arguments.size()) {
        napi_throw_type_error(
            env, nullptr,
            "submitKeyEvent requires keyCode, pressed and modifiers");
        return nullptr;
    }

    std::int32_t keyCode = 0;
    bool pressed = false;
    bool alt = false;
    bool control = false;
    bool shift = false;
    bool system = false;
    if (napi_get_value_int32(env, arguments[0], &keyCode) != napi_ok ||
        !readBoolean(env, arguments[1], pressed) ||
        !readBoolean(env, arguments[2], alt) ||
        !readBoolean(env, arguments[3], control) ||
        !readBoolean(env, arguments[4], shift) ||
        !readBoolean(env, arguments[5], system)) {
        napi_throw_type_error(
            env, nullptr,
            "submitKeyEvent requires a numeric keyCode and boolean state");
        return nullptr;
    }

    const OH_NativeXComponent_KeyCode nativeKeyCode =
        static_cast<OH_NativeXComponent_KeyCode>(keyCode);
    const sf::Keyboard::Key key =
        sf::priv::Harmony::keyCodeToKey(nativeKeyCode);
    const sf::Keyboard::Scancode scan =
        sf::priv::Harmony::keyCodeToScancode(nativeKeyCode);
    if (nativeKeyCode == KEY_BACK ||
        (key == sf::Keyboard::Key::Unknown &&
         scan == sf::Keyboard::Scancode::Unknown)) {
        napi_value result{};
        return napi_get_boolean(env, false, &result) == napi_ok ? result
                                                                : nullptr;
    }

    InjectedInputEvent event;
    event.type = pressed ? "KeyPressed" : "KeyReleased";
    event.scan = static_cast<int>(scan);
    event.alt = alt;
    event.control = control;
    event.shift = shift;
    event.system = system;
    inputService().injectEvent(event);

    napi_value result{};
    return napi_get_boolean(env, true, &result) == napi_ok ? result : nullptr;
}

napi_value submitSystemBack(napi_env env, napi_callback_info) {
    InputService::requestSystemCancel();
    napi_value result{};
    return napi_get_boolean(env, true, &result) == napi_ok ? result : nullptr;
}

}  // namespace

extern "C" napi_value initializeModule(napi_env env, napi_value exports) {
    napi_value initializedExports =
        sf::priv::Harmony::initializeNativeApp(env, exports);
    if (initializedExports == nullptr) {
        return nullptr;
    }

    const std::array properties{
        napi_property_descriptor{"configureRuntimePaths", nullptr,
                                 ::configureRuntimePaths, nullptr, nullptr,
                                 nullptr, napi_default, nullptr},
        napi_property_descriptor{"configureSystemLocale", nullptr,
                                 ::configureSystemLocale, nullptr, nullptr,
                                 nullptr, napi_default, nullptr},
        napi_property_descriptor{"configureDisplayScaleHost", nullptr,
                                 ::configureDisplayScaleHost, nullptr, nullptr,
                                 nullptr, napi_default, nullptr},
        napi_property_descriptor{"clearDisplayScaleHost", nullptr,
                                 ::clearDisplayScaleHost, nullptr, nullptr,
                                 nullptr, napi_default, nullptr},
        napi_property_descriptor{"submitKeyEvent", nullptr, ::submitKeyEvent,
                                 nullptr, nullptr, nullptr, napi_default,
                                 nullptr},
        napi_property_descriptor{"submitSystemBack", nullptr,
                                 ::submitSystemBack, nullptr, nullptr, nullptr,
                                 napi_default, nullptr}};
    if (napi_define_properties(env, initializedExports, properties.size(),
                               properties.data()) != napi_ok) {
        napi_throw_error(env, nullptr,
                         "Failed to define the Ludork Harmony exports");
        return nullptr;
    }
    if (napi_add_env_cleanup_hook(env, cleanupDisplayScaleHost, env) !=
        napi_ok) {
        napi_throw_error(env, nullptr,
                         "Failed to register the Ludork Harmony cleanup");
        return nullptr;
    }

    return initializedExports;
}

static napi_module entryModule{
    1,
    0,
    nullptr,
    initializeModule,
    "entry",
    nullptr,
    {nullptr, nullptr, nullptr, nullptr},
};

extern "C" __attribute__((constructor)) void registerEntryModule() {
    napi_module_register(&entryModule);
}
