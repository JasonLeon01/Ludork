#pragma once
#include <Runtime/Graphics/ShaderCache.hpp>

namespace ludork::engine::materials {

class MaterialCachePreparation {
public:
    using Projection =
        ludork::runtime::graphics::ShaderCache::MaterialProjection;
    using PlannedAction =
        std::function<void(const Projection*, const std::function<void()>&)>;

    static void prepare(const std::filesystem::path& root,
                        const std::atomic<bool>& stop,
                        const std::function<void(double)>& progress,
                        const PlannedAction& action);
};

}  // namespace ludork::engine::materials
