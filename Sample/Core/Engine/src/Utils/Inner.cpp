#include <Utils/Inner.hpp>

#include <LudorkPlatform.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace {

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

std::string appName;
std::unordered_set<std::string> warnings;

std::string trim(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char character) {
                                            return std::isspace(character) != 0;
                                        });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char character) {
                                           return std::isspace(character) != 0;
                                       })
                          .base();
    return first < last ? std::string(first, last) : std::string{};
}

std::string environmentValue(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

std::filesystem::path homePath() {
    std::string value = environmentValue("HOME");
    if (value.empty()) {
        value = environmentValue("USERPROFILE");
    }
    if (value.empty()) {
        throw std::runtime_error("User home directory is not configured");
    }
    return ludork::standard::pathFromUtf8(value);
}

std::string resolveAppName(const std::optional<std::string>& appNameOverride) {
    return appNameOverride.has_value() ? *appNameOverride : getAppName();
}

std::filesystem::path userDataPath(
    const std::optional<std::string>& appNameOverride) {
#if defined(_WIN32)
    static_cast<void>(appNameOverride);
    return std::filesystem::current_path();
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    const std::filesystem::path resolvedName =
        ludork::standard::pathFromUtf8(resolveAppName(appNameOverride));
    const std::filesystem::path path =
        homePath() / "Library" / "Application Support" / resolvedName;
    std::filesystem::create_directories(path);
    return path;
#elif defined(SFML_SYSTEM_HARMONY) || defined(SFML_SYSTEM_ANDROID)
    const std::string root = environmentValue("LUDORK_USER_DATA_ROOT");
    if (root.empty()) {
        throw std::runtime_error("User data root is not configured");
    }
    const std::filesystem::path resolvedName =
        ludork::standard::pathFromUtf8(resolveAppName(appNameOverride));
    const std::filesystem::path path =
        ludork::standard::pathFromUtf8(root) / resolvedName;
    std::filesystem::create_directories(path);
    return path;
#else
    const std::filesystem::path resolvedName =
        ludork::standard::pathFromUtf8(resolveAppName(appNameOverride));
    const std::filesystem::path path = homePath() / resolvedName;
    std::filesystem::create_directories(path);
    return path;
#endif
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
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("reflect.tostring", {value});
    if (!resolved.empty()) {
        if (const std::string* text = resolved.front().getIf<std::string>()) {
            return *text;
        }
    }
    return value.typeName();
}

}  // namespace

void setAppName(const std::string& value) {
    const std::string normalized = trim(value);
    if (normalized.empty()) {
        throw std::invalid_argument("Application name must not be empty");
    }
    appName = normalized;
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
#if !defined(_WIN32)
    std::filesystem::create_directories(path);
#endif
    return ludork::standard::pathToUtf8(path);
}

std::string getMainIniPath(const std::optional<std::string>& appNameOverride) {
    return ludork::standard::pathToUtf8(userDataPath(appNameOverride) /
                                        "Main.ini");
}

std::string getAnimationSourceRoot() {
    return ludork::standard::pathToUtf8(std::filesystem::path(".") / "Data" /
                                        "Animations");
}

std::string getAnimationCacheRoot(
    const std::optional<std::string>& appNameOverride) {
#if defined(_WIN32)
    static_cast<void>(appNameOverride);
    return getAnimationSourceRoot();
#else
    const std::filesystem::path path =
        userDataPath(appNameOverride) / "Data" / "Animations";
    std::filesystem::create_directories(path);
    return ludork::standard::pathToUtf8(path);
#endif
}

RuntimeValue::Map filterDataClassParams(const RuntimeValue::Map& params,
                                        const RuntimeValue& type) {
    RuntimeValue::Map result;
    for (const auto& [key, value] : params) {
        const std::vector<RuntimeValue> member =
            resolveRuntime("reflect.get", {type, RuntimeValue(key)});
        if (!member.empty() && !member.front().isNil()) {
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
