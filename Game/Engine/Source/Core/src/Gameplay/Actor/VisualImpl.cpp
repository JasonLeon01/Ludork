#include "VisualImpl.hpp"

#include <CoreShared/SpriteVisuals.hpp>

namespace ludork::engine::actor_impl {

std::shared_ptr<const ludork::engine::materials::MaterialResource>
VisualImpl::getMaterial(const std::string& path) {
    if (!material_ || materialPath_ != path) {
        material_ = ludork::engine::materials::MaterialResource::load(path);
        materialPath_ = path;
    }
    return material_;
}

bool VisualImpl::advanceAnimation(float deltaTime, float interval) {
    switchTimer_ += deltaTime;
    if (switchTimer_ < interval) {
        return false;
    }
    switchTimer_ = 0.0f;
    return true;
}

}  // namespace ludork::engine::actor_impl
