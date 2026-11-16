#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>

#include <vector>

namespace ludork::runtime::graphics {

class LUDORK_RUNTIME_API GpuEmitterShaders {
public:
    enum class Kind {
        Update,
        Draw,
        Mask,
        Reduction
    };

    static std::vector<ShaderCache::Program> programs();
    static sf::BlendMode blendMode(bool additive);
};

}  // namespace ludork::runtime::graphics
