#pragma once

#include <Runtime/AssetPath.hpp>

#include <string>

namespace ludork::global::manager {

inline std::string assetFile(const std::string& folder,
                             const std::string& filename) {
    return ludork::runtime::makeAssetPath(folder, filename);
}

inline std::string textureAssetFile(const std::string& folder,
                                    std::string filename) {
    const std::size_t separator = filename.rfind('/');
    const std::size_t extension = filename.rfind('.');
    if (extension == std::string::npos ||
        (separator != std::string::npos && extension < separator)) {
        filename += ".png";
    } else if (extension + 1 == filename.size()) {
        filename += "png";
    }
    return assetFile(folder, filename);
}

}  // namespace ludork::global::manager
