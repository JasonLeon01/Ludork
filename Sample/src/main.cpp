#include <Application.hpp>
#include <SFML/Config.hpp>

#include <cstdlib>

#if defined(SFML_SYSTEM_ANDROID)
#include <SFML/System/NativeActivity.hpp>

#include <android/native_activity.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#endif

#if defined(SFML_SYSTEM_IOS) || defined(SFML_SYSTEM_HARMONY) ||              \
    defined(SFML_SYSTEM_ANDROID)
#include <SFML/Main.hpp>
#endif

namespace {

#if defined(SFML_SYSTEM_ANDROID)
void configureAndroidRuntimePaths() {
    ANativeActivity* activity = sf::getNativeActivity();
    if (activity == nullptr || activity->internalDataPath == nullptr) {
        throw std::runtime_error(
            "Android internal application storage is unavailable");
    }
    const std::filesystem::path filesRoot(activity->internalDataPath);
    const std::filesystem::path ludorkRoot = filesRoot / "ludork";
    ludork::application::configureRuntimePaths(
        ludorkRoot / "runtime" / LUDORK_ANDROID_RUNTIME_HASH,
        ludorkRoot / "user-data");
    std::ifstream localeFile(ludorkRoot / "system-locale");
    if (!localeFile) {
        throw std::runtime_error("Android system locale is unavailable");
    }
    std::string systemLocale;
    std::getline(localeFile, systemLocale);
    ludork::application::configureSystemLocale(systemLocale);
}
#endif

}

int main(int argc, char** argv) {
#if defined(SFML_SYSTEM_ANDROID)
    configureAndroidRuntimePaths();
#endif
    const int result = ludork::application::run(argc, argv);
#if defined(SFML_SYSTEM_IOS)
    std::exit(result);
#else
    return result;
#endif
}
