#include <Utils/Inner.hpp>
#include <ConfigParser.hpp>
#include <StringUtils.hpp>

#include <LudorkPlatform.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_set>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace {

std::string appName;
std::unordered_set<std::string> warnings;

#if !defined(_WIN32)
std::string environmentValue(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}
#endif

std::filesystem::path homePath() {
#if defined(_WIN32)
    PWSTR value = nullptr;
    const HRESULT result = SHGetKnownFolderPath(
        FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &value);
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> owner(
        value, &CoTaskMemFree);
    if (FAILED(result) || value == nullptr || *value == L'\0') {
        throw std::runtime_error(
            "Current user profile directory is unavailable");
    }
    const std::filesystem::path path(value);
#else
    const std::string value = environmentValue("HOME");
    if (value.empty()) {
        throw std::runtime_error("User home directory is not configured");
    }
    const std::filesystem::path path = ludork::standard::pathFromUtf8(value);
#endif
    if (!path.is_absolute()) {
        throw std::runtime_error(
            "User home directory must be an absolute path");
    }
    return path;
}

std::string normalizeAppName(const std::string& value) {
    const std::string normalized =
        ludork::standard::trimCharacters(value, " \t\r\n\v\f");
    if (normalized.empty() || normalized == "." || normalized == ".." ||
        normalized.find_first_of("/\\:") != std::string::npos ||
        std::any_of(normalized.begin(), normalized.end(),
                    [](unsigned char character) {
                        return character < 32 || character == 127;
                    })) {
        throw std::invalid_argument(
            "Application name must be a non-empty directory name without "
            "path separators, colons or control characters");
    }
    return normalized;
}

std::string resolveAppName(const std::optional<std::string>& appNameOverride) {
    return appNameOverride.has_value() ? normalizeAppName(*appNameOverride)
                                       : getAppName();
}

std::filesystem::path userDataPath(
    const std::optional<std::string>& appNameOverride) {
    const std::filesystem::path resolvedName =
        ludork::standard::pathFromUtf8(resolveAppName(appNameOverride));
#if defined(__APPLE__) && TARGET_OS_IPHONE
    const std::filesystem::path root =
        homePath() / "Library" / "Application Support";
#elif defined(SFML_SYSTEM_HARMONY) || defined(SFML_SYSTEM_ANDROID)
    const std::string rootValue = environmentValue("LUDORK_USER_DATA_ROOT");
    if (rootValue.empty()) {
        throw std::runtime_error("User data root is not configured");
    }
    const std::filesystem::path root =
        ludork::standard::pathFromUtf8(rootValue);
#else
    const std::filesystem::path root = homePath();
#endif
    const std::filesystem::path path = root / resolvedName;
    std::filesystem::create_directories(path);
    return path;
}

std::string runtimeValueString(const RuntimeValue& value) {
    if (const std::string* text = value.getIf<std::string>()) {
        return *text;
    }
    if (const bool* boolean = value.getIf<bool>()) {
        return *boolean ? "true" : "false";
    }
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return std::to_string(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        std::array<char, 64> buffer{};
#if defined(__APPLE__) && TARGET_OS_IPHONE
        const int length =
            std::snprintf(buffer.data(), buffer.size(), "%.17g", *number);
        if (length > 0 && static_cast<std::size_t>(length) < buffer.size()) {
            std::string result(buffer.data(), static_cast<std::size_t>(length));
            if (std::isfinite(*number) &&
                result.find_first_of(".eE") == std::string::npos) {
                result += ".0";
            }
            return result;
        }
#else
        const auto [ending, error] =
            std::to_chars(buffer.data(), buffer.data() + buffer.size(), *number,
                          std::chars_format::general);
        if (error == std::errc{}) {
            std::string result(buffer.data(), ending);
            if (std::isfinite(*number) &&
                result.find_first_of(".eE") == std::string::npos) {
                result += ".0";
            }
            return result;
        }
#endif
    }
    if (value.isNil()) {
        return "nil";
    }
    return runtimeReflection().toString(value);
}

}  // namespace

void setAppName(const std::string& value) {
    appName = normalizeAppName(value);
}

const std::string& getAppName() {
    if (appName.empty()) {
        throw std::logic_error(
            "Application name is not configured. Call setAppName from Entry "
            "first");
    }
    return appName;
}

void warnOnce(const std::string& key, const std::string& message) {
    if (warnings.insert(key).second) {
        std::cerr << message << '\n';
    }
}

std::string getUserDataPath(const std::optional<std::string>& appNameOverride) {
    return ludork::standard::pathToUtf8(userDataPath(appNameOverride));
}

std::string getSavePath(const std::optional<std::string>& appNameOverride) {
    const std::filesystem::path path = userDataPath(appNameOverride) / "Save";
    std::filesystem::create_directories(path);
    return ludork::standard::pathToUtf8(path);
}

std::string getMainConfigPath(
    const std::optional<std::string>& appNameOverride) {
    return ludork::standard::pathToUtf8(
        userDataPath(appNameOverride) /
        ludork::standard::ConfigParser::defaultFileName("Main"));
}

std::string getAnimationSourceRoot() {
    return ludork::standard::pathToUtf8(std::filesystem::path(".") / "Data" /
                                        "Animations");
}

std::string getAnimationCacheRoot(
    const std::optional<std::string>& appNameOverride) {
    const std::filesystem::path path =
        userDataPath(appNameOverride) / "Cache" / "Animations";
    std::filesystem::create_directories(path);
    return ludork::standard::pathToUtf8(path);
}

bool assetExists(const std::string& assetPath) {
    return ludork::runtime::assetStore().exists(assetPath);
}

double assetModificationTime(const std::string& assetPath) {
    const std::optional<ludork::runtime::AssetStore::AssetStat> stat =
        ludork::runtime::assetStore().stat(assetPath);
    if (!stat.has_value()) {
        throw std::runtime_error("Asset not found: " + assetPath);
    }
    return stat->modificationTime;
}

RuntimeValue::Map filterDataClassParams(const RuntimeValue::Map& params,
                                        const RuntimeValue& type) {
    RuntimeValue::Map result;
    for (const auto& [key, value] : params) {
        const RuntimeValue member = runtimeReflection().get(
            ludork::runtime::reference::intern(type), key);
        if (!member.isNil()) {
            result.emplace(key, value);
        }
    }
    return result;
}

RuntimeValue applyStringMappingFormat(const RuntimeValue& value,
                                      const RuntimeValue::Map& values) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        return value;
    }
    std::string result;
    result.reserve(text->size());
    std::size_t position = 0;
    while (position < text->size()) {
        const std::size_t open = text->find('{', position);
        if (open == std::string::npos) {
            result.append(*text, position, std::string::npos);
            break;
        }
        result.append(*text, position, open - position);
        if (open + 1 < text->size() && (*text)[open + 1] == '{') {
            result.push_back('{');
            position = open + 2;
            continue;
        }
        const std::size_t close = text->find('}', open + 1);
        if (close == std::string::npos) {
            result.append(*text, open, std::string::npos);
            break;
        }
        const std::string key = text->substr(open + 1, close - open - 1);
        const auto replacement = values.find(key);
        if (replacement == values.end()) {
            result.append(*text, open, close - open + 1);
        } else {
            result.append(runtimeValueString(replacement->second));
        }
        position = close + 1;
    }
    std::string unescaped;
    unescaped.reserve(result.size());
    for (std::size_t index = 0; index < result.size(); ++index) {
        if (result[index] == '}' && index + 1 < result.size() &&
            result[index + 1] == '}') {
            unescaped.push_back('}');
            ++index;
        } else {
            unescaped.push_back(result[index]);
        }
    }
    return RuntimeValue(std::move(unescaped));
}

void shutdownInner() noexcept {
    appName.clear();
    warnings.clear();
}
