#pragma once

#include <StandardApi.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace ludork::standard {

LUDORK_STANDARD_API std::filesystem::path pathFromUtf8(std::string_view utf8);

LUDORK_STANDARD_API std::string pathToUtf8(const std::filesystem::path& path);

LUDORK_STANDARD_API std::string pathToGenericUtf8(
    const std::filesystem::path& path);

}  // namespace ludork::standard
