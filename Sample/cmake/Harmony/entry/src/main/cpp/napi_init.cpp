#include <Application.hpp>
#include <Input/InputService.hpp>
#include <SFML/Main/MainHarmony.hpp>

#include <array>
#include <exception>
#include <filesystem>
#include <napi/native_api.h>
#include <string>
#include <vector>

namespace {

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
        napi_property_descriptor{"submitSystemBack", nullptr,
                                 ::submitSystemBack, nullptr, nullptr, nullptr,
                                 napi_default, nullptr}};
    if (napi_define_properties(env, initializedExports, properties.size(),
                               properties.data()) != napi_ok) {
        napi_throw_error(env, nullptr,
                         "Failed to define the Ludork Harmony exports");
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
