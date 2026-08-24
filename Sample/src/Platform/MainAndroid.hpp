#pragma once

#include <Input/InputService.hpp>
#include <SFML/System/NativeActivity.hpp>

#include <android/native_activity.h>
#include <jni.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

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

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_ludork_android_LudorkActivity_submitSystemBack(JNIEnv*, jclass) {
    InputService::requestSystemCancel();
}

#define LUDORK_DEFINE_MAIN()                         \
    int main(int argc, char** argv) {                \
        configureAndroidRuntimePaths();              \
        return ludork::application::run(argc, argv); \
    }
