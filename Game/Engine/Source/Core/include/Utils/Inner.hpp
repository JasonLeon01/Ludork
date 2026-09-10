#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

BIND_FUNCTION(name = "setAppName")
void setAppName(const std::string& value);

BIND_FUNCTION(name = "getAppName")
const std::string& getAppName();

BIND_FUNCTION(name = "warnOnce")
LUDORK_ENGINE_API void warnOnce(const std::string& key,
                                const std::string& message);

BIND_FUNCTION(name = "getUserDataPath")
std::string getUserDataPath(
    const std::optional<std::string>& appNameOverride = std::nullopt);

BIND_FUNCTION(name = "getSavePath")
std::string getSavePath(
    const std::optional<std::string>& appNameOverride = std::nullopt);

BIND_FUNCTION(name = "getMainIniPath")
std::string getMainIniPath(
    const std::optional<std::string>& appNameOverride = std::nullopt);

BIND_FUNCTION(name = "getAnimationSourceRoot")
std::string getAnimationSourceRoot();

BIND_FUNCTION(name = "getAnimationCacheRoot")
std::string getAnimationCacheRoot(
    const std::optional<std::string>& appNameOverride = std::nullopt);

BIND_FUNCTION(name = "assetExists", metadata = false)
bool assetExists(const std::string& assetPath);

BIND_FUNCTION(name = "assetModificationTime", metadata = false)
double assetModificationTime(const std::string& assetPath);

BIND_FUNCTION(name = "filterDataClassParams")
RuntimeValue::Map filterDataClassParams(const RuntimeValue::Map& params,
                                        const RuntimeValue& type);

BIND_FUNCTION(name = "ApplyStringMappingFormat")
RuntimeValue applyStringMappingFormat(const RuntimeValue& value,
                                      const RuntimeValue::Map& values);

LUDORK_ENGINE_API void shutdownInner() noexcept;
