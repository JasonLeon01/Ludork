#pragma once

#include <BindAnnotations.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <filesystem>
#include <string>

LUDORK_ENGINE_API RuntimeValue parseJSONText(const std::string& text);
LUDORK_ENGINE_API std::string stringifyJSON(const RuntimeValue& value);
LUDORK_ENGINE_API RuntimeValue getJSONData(
    const std::filesystem::path& filePath);
LUDORK_ENGINE_API std::string getJSONText(
    const std::filesystem::path& filePath);
LUDORK_ENGINE_API bool jsonExists(const std::filesystem::path& filePath);
LUDORK_ENGINE_API void writeJSON(const std::filesystem::path& filePath,
                                 const RuntimeValue& value);

BIND_FUNCTION(name = "getJSONData")
LUDORK_ENGINE_API RuntimeValue getJSONData(const std::string& filePath);

BIND_FUNCTION(name = "getJSONText")
LUDORK_ENGINE_API std::string getJSONText(const std::string& filePath);

BIND_FUNCTION(name = "jsonExists")
LUDORK_ENGINE_API bool jsonExists(const std::string& filePath);

BIND_FUNCTION(name = "writeJSON")
LUDORK_ENGINE_API void writeJSON(const std::string& filePath,
                                 const RuntimeValue& value);
