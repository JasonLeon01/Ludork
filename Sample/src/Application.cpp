#include <Application.hpp>

#include "ApplicationPaths.hpp"
#include "ApplicationPlatform.hpp"
#include "ApplicationRuntime.hpp"

#include <GlobalRuntimeApi.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/DataStore.hpp>
#include <Runtime/ScriptStore.hpp>
#include <SystemServices.hpp>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

bool parseRuntimeLaunchOptions(ludork::global::RuntimeLaunchOptions& options,
                               std::string& error) {
    const char* editor = std::getenv("LUDORK_EDITOR");
    options.editor = editor != nullptr && std::string_view(editor) == "1";

    const char* mode = std::getenv("LUDORK_WINDOW_MODE");
    const std::string_view modeValue = mode == nullptr ? "" : mode;
    if (modeValue.empty()) {
        options.windowMode = ludork::global::RuntimeWindowMode::PlatformDefault;
    } else if (modeValue == "embedded") {
        options.windowMode = ludork::global::RuntimeWindowMode::Embedded;
    } else if (modeValue == "individual") {
        options.windowMode = ludork::global::RuntimeWindowMode::Individual;
    } else {
        error = "Invalid LUDORK_WINDOW_MODE: " + std::string(modeValue);
        return false;
    }

    if (options.windowMode != ludork::global::RuntimeWindowMode::Embedded) {
        return true;
    }
    return ludork::application::detail::configureEmbeddedHostWindow(options,
                                                                    error);
}

}  // namespace

namespace ludork::application {

void configureSystemLocale(const std::string& systemLocale) {
    if (systemLocale.empty()) {
        throw std::invalid_argument("System locale must not be empty");
    }
    ludork::standard::setDefaultLocale(systemLocale);
}

int run(int argc, char** argv) {
    const std::filesystem::path executablePath = detail::normalizedAbsolutePath(
        argc > 0 && argv[0] != nullptr ? argv[0] : "");
    std::filesystem::path runtimeRoot;
    if (!detail::useRuntimeRoot(executablePath, runtimeRoot)) {
        return 1;
    }
    try {
        ludork::runtime::scriptStore().configure(runtimeRoot);
        const bool scriptsPacked = ludork::runtime::scriptStore().mode() ==
                                   ludork::runtime::ScriptStoreMode::Packed;
        ludork::runtime::assetStore().configure(
            runtimeRoot, scriptsPacked
                             ? ludork::runtime::AssetStoreMode::Packed
                             : ludork::runtime::AssetStoreMode::Loose);
        ludork::runtime::dataStore().configure(
            runtimeRoot, scriptsPacked ? ludork::runtime::DataStoreMode::Packed
                                       : ludork::runtime::DataStoreMode::Loose);
        const bool assetsPacked = ludork::runtime::assetStore().mode() ==
                                  ludork::runtime::AssetStoreMode::Packed;
        const bool dataPacked = ludork::runtime::dataStore().mode() ==
                                ludork::runtime::DataStoreMode::Packed;
        if (assetsPacked != dataPacked || assetsPacked != scriptsPacked) {
            throw std::runtime_error(
                "Assets, Data, and Scripts must all use the same loose or "
                "packed layout");
        }
    } catch (const std::exception& error) {
        detail::reportStartupError(error.what());
        return 1;
    }

    global::RuntimeLaunchOptions launchOptions;
    std::string launchError;
    if (!parseRuntimeLaunchOptions(launchOptions, launchError)) {
        detail::reportStartupError(launchError);
        return 1;
    }
    global::setRuntimeLaunchOptions(launchOptions);

    return detail::runRuntime(executablePath, argc, argv);
}

}  // namespace ludork::application
