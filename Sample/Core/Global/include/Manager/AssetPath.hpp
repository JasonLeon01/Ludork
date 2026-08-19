#pragma once

#include <Utf8Path.hpp>

#include <filesystem>
#include <string>

namespace ludork::global::manager {

inline std::string assetFile(const std::string& folder,
                             const std::string& filename) {
    return ludork::standard::pathToUtf8(
        std::filesystem::path("Assets") /
        ludork::standard::pathFromUtf8(folder) /
        ludork::standard::pathFromUtf8(filename));
}

inline std::string textureAssetFile(const std::string& folder,
                                    std::string filename) {
    const std::filesystem::path path = ludork::standard::pathFromUtf8(filename);
    const std::string extension =
        ludork::standard::pathToUtf8(path.extension());
    if (extension.empty()) {
        filename += ".png";
    } else if (extension == ".") {
        filename += "png";
    }
    return assetFile(folder, filename);
}

}  // namespace ludork::global::manager
