#include "GraphicsConfigImpl.hpp"
#include "ConfigStoreImpl.hpp"

#include <Graphics.hpp>
#include <EngineState.hpp>
#include <algorithm>
#include <cmath>

namespace ludork::global::system_impl {

namespace {
constexpr float DefaultMaximumRenderScale = 2.0f;
constexpr float DefaultLightingRenderScale = 1.0f;
}  // namespace

float GraphicsConfigImpl::maximumRenderScale_ = DefaultMaximumRenderScale;
float GraphicsConfigImpl::lightingRenderScale_ = DefaultLightingRenderScale;
bool GraphicsConfigImpl::initialized_ = false;

void GraphicsConfigImpl::initialize() {
    maximumRenderScale_ = normalizeMaximumRenderScale(
        static_cast<float>(ConfigStoreImpl::data()
                               .getFloat("Main", "maxrenderscale")
                               .value_or(maximumRenderScale_)));
    lightingRenderScale_ = normalizeLightingRenderScale(
        static_cast<float>(ConfigStoreImpl::data()
                               .getFloat("Main", "lightingrenderscale")
                               .value_or(lightingRenderScale_)));
    initialized_ = true;
}

void GraphicsConfigImpl::initializeRenderScale(float configuredScale) {
    const float initialScale = configuredScale > 0.0f ? configuredScale : 1.0f;
    engineState().setScale(maximumRenderScale_ > 0.0f
                               ? std::min(initialScale, maximumRenderScale_)
                               : initialScale);
}

void GraphicsConfigImpl::shutdown() noexcept {
    initialized_ = false;
    maximumRenderScale_ = DefaultMaximumRenderScale;
    lightingRenderScale_ = DefaultLightingRenderScale;
}

float GraphicsConfigImpl::getMaximumRenderScale() {
    return maximumRenderScale_;
}

void GraphicsConfigImpl::setMaximumRenderScale(float value) {
    maximumRenderScale_ = normalizeMaximumRenderScale(value);
    saveMaximumRenderScale(maximumRenderScale_);
    if (initialized_) {
        Graphics::onConfigurationChanged("maximumRenderScale");
    }
}

void GraphicsConfigImpl::saveMaximumRenderScale(float value) {
    ConfigStoreImpl::setIniData("maxrenderscale",
                                normalizeMaximumRenderScale(value));
}

float GraphicsConfigImpl::getLightingRenderScale() {
    return lightingRenderScale_;
}

void GraphicsConfigImpl::setLightingRenderScale(float value) {
    lightingRenderScale_ = normalizeLightingRenderScale(value);
    saveLightingRenderScale(lightingRenderScale_);
    if (initialized_) {
        Graphics::onConfigurationChanged("lightingRenderScale");
    }
}

void GraphicsConfigImpl::saveLightingRenderScale(float value) {
    ConfigStoreImpl::setIniData("lightingrenderscale",
                                normalizeLightingRenderScale(value));
}

float GraphicsConfigImpl::normalizeMaximumRenderScale(float scale) {
    return std::isfinite(scale) && scale >= 0.0f ? scale : 2.0f;
}

float GraphicsConfigImpl::normalizeLightingRenderScale(float scale) {
    if (scale == 0.5f || scale == 0.75f || scale == 1.0f) {
        return scale;
    }
    return 1.0f;
}

}  // namespace ludork::global::system_impl
