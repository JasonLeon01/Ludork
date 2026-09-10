#pragma once

#include <RuntimeApi.hpp>

#include <string>

namespace ludork::runtime {

struct LUDORK_RUNTIME_API AssetPath {
    std::string value;
    std::string group;
    std::string relativePath;

    [[nodiscard]] static AssetPath parse(const std::string& value);
};

[[nodiscard]] LUDORK_RUNTIME_API std::string makeAssetPath(
    const std::string& group, const std::string& relativePath);

}  // namespace ludork::runtime
