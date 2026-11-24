#pragma once
#include <Runtime/Graphics/ShaderCache.hpp>

namespace ludork::engine::shaders {

void prepareShaderPrograms(
    const std::filesystem::path& userDataRoot,
    const std::vector<std::vector<std::string>>& additionalPrograms,
    const std::atomic<bool>& stop, const std::function<void(double)>& progress,
    const ludork::runtime::graphics::ShaderCache::MaterialProjection*
        projectedMaterials = nullptr,
    const std::function<void(bool)>& onPlanned = {});

}  // namespace ludork::engine::shaders
