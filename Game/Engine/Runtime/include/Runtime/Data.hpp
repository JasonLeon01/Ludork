#pragma once

#include <Runtime/RuntimeData.hpp>
#include <RuntimeApi.hpp>

#include <filesystem>
#include <string>
#include <string_view>

LUDORK_RUNTIME_API RuntimeData parseBinaryData(std::string_view bytes);
LUDORK_RUNTIME_API RuntimeData loadData(const std::filesystem::path& path);
LUDORK_RUNTIME_API void saveData(const std::filesystem::path& path,
                                 const RuntimeData& value);
LUDORK_RUNTIME_API bool dataExists(const std::filesystem::path& path);

/// Loads JSON or binary LDC data. A missing .json path resolves to its .ldc
/// alternative.
BIND_FUNCTION(name = "loadData")
LUDORK_RUNTIME_API RuntimeData loadData(const std::string& path);

/// Writes JSON for .json paths or binary LDC for .ldc paths.
BIND_FUNCTION(name = "saveData")
LUDORK_RUNTIME_API void saveData(const std::string& path,
                                 const RuntimeData& value);

/// Checks the data path, including the .ldc alternative for a missing .json
/// file.
BIND_FUNCTION(name = "dataExists")
LUDORK_RUNTIME_API bool dataExists(const std::string& path);

/// Decodes complete JSON or LDC file bytes without an intermediate JSON
/// representation.
BIND_FUNCTION(name = "decodeData")
LUDORK_RUNTIME_API RuntimeData decodeData(const std::string& bytes);
