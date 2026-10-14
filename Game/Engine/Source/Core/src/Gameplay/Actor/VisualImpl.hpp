#pragma once

#include <Graphics/Materials/MaterialResource.hpp>

#include <memory>
#include <string>

namespace ludork::engine::actor_impl {

class VisualImpl {
public:
    std::shared_ptr<const ludork::engine::materials::MaterialResource>
    getMaterial(const std::string& path);
    bool advanceAnimation(float deltaTime, float interval);

private:
    float switchTimer_ = 0.0f;
    std::shared_ptr<const ludork::engine::materials::MaterialResource>
        material_;
    std::string materialPath_;
};

}  // namespace ludork::engine::actor_impl
