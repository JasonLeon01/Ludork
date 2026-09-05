#pragma once

#include <CoreMinimal.hpp>

#include <RuntimeApi.hpp>

#include <filesystem>

LUDORK_RUNTIME_API RuntimeData parseJSONText(const std::string& text);
LUDORK_RUNTIME_API std::string stringifyJSON(const RuntimeData& value);
LUDORK_RUNTIME_API RuntimeData
getJSONData(const std::filesystem::path& filePath);
LUDORK_RUNTIME_API std::string getJSONText(
    const std::filesystem::path& filePath);
LUDORK_RUNTIME_API bool jsonExists(const std::filesystem::path& filePath);
LUDORK_RUNTIME_API void writeJSON(const std::filesystem::path& filePath,
                                  const RuntimeData& value);

LUDORK_RUNTIME_API RuntimeData getJSONData(const std::string& filePath);

BIND_FUNCTION(name = "getJSONText")
LUDORK_RUNTIME_API std::string getJSONText(const std::string& filePath);

BIND_FUNCTION(name = "jsonExists")
LUDORK_RUNTIME_API bool jsonExists(const std::string& filePath);

LUDORK_RUNTIME_API void writeJSON(const std::string& filePath,
                                  const RuntimeData& value);

BIND_FUNCTION(name = "getJSONData")
LUDORK_RUNTIME_API RuntimeValue getJSONDataForLua(const std::string& filePath);

BIND_FUNCTION(name = "writeJSON")
LUDORK_RUNTIME_API void writeJSONForLua(const std::string& filePath,
                                        const RuntimeValue& value);
