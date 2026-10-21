#include <Graphics/MapRenderer.hpp>
#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <Utils/ShaderLoader.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

MapRenderer::MapRenderer() : impl_(std::make_unique<Impl>()) {}
MapRenderer::~MapRenderer() = default;

const sf::RenderTexture& MapRenderer::render(const Scene& scene,
                                             const View& view,
                                             const Options& options,
                                             const DrawHooks& hooks,
                                             const sf::Texture* overlay) {
    return impl_->render(scene, view, options, hooks, overlay);
}

void MapRenderer::drawContent(const Scene& scene, sf::RenderTarget& target,
                              const sf::RenderStates& states,
                              const Options& options, const DrawHooks& hooks) {
    impl_->drawContent(scene, target, states, options, hooks);
}

void MapRenderer::drawSprite(const Sprite& sprite, sf::RenderTarget& target,
                             const sf::RenderStates& states, float time) {
    impl_->drawSprite(sprite, target, states, time);
}

const MapRenderer::Statistics& MapRenderer::statistics() const {
    return impl_->statistics_;
}

void MapRenderer::invalidateScene() noexcept {
    impl_->transmissionValid_ = false;
}

void MapRenderer::reset() {
    impl_ = std::make_unique<Impl>();
}

sf::RenderTexture& MapRenderer::Impl::target(
    std::unique_ptr<sf::RenderTexture>& value, sf::Vector2u size) {
    if (!value || value->getSize() != size) {
        value = std::make_unique<sf::RenderTexture>(size);
        ++statistics_.renderTextureAllocations;
        value->setSmooth(false);
    }
    return *value;
}

std::shared_ptr<sf::Shader> MapRenderer::Impl::shader(const std::string& path) {
    const auto found = shaders_.find(path);
    if (found != shaders_.end()) {
        return found->second;
    }
    ShaderLoadResult result =
        ShaderLoader::load(path, sf::Shader::Type::Fragment);
    if (!result) {
        throw std::runtime_error(result.error);
    }
    return shaders_.emplace(path, std::move(result.shader)).first->second;
}

sf::View MapRenderer::Impl::nativeView(const View& view) {
    sf::View result(view.worldRect.position + view.worldRect.size * 0.5f,
                    view.worldRect.size);
    result.setRotation(view.rotation);
    return result;
}

sf::Vector3f MapRenderer::Impl::colour(const sf::Color& value, bool alpha) {
    const float factor =
        (alpha ? static_cast<float>(value.a) / 255.0f : 1.0f) / 255.0f;
    return {value.r * factor, value.g * factor, value.b * factor};
}

void MapRenderer::Impl::setViewUniforms(sf::Shader& shader, const Scene& scene,
                                        const View& view) {
    const float angle = view.rotation.asRadians();
    shader.setUniform("screenSize", view.worldRect.size);
    shader.setUniform("viewPos", view.worldRect.position);
    shader.setUniform("viewSinCos",
                      sf::Vector2f(std::sin(angle), std::cos(angle)));
    shader.setUniform("gridSize", sf::Vector2f(scene.cellSize));
    shader.setUniform("cellSize", static_cast<float>(EngineState::CellSize));
}

void MapRenderer::Impl::validate(const Scene& scene, const View& view,
                                 const Options& options) {
    if (scene.cellSize.x == 0 || scene.cellSize.y == 0 ||
        view.pixelSize.x == 0 || view.pixelSize.y == 0 ||
        !std::isfinite(view.worldRect.size.x) ||
        !std::isfinite(view.worldRect.size.y) ||
        view.worldRect.size.x <= 0.0f || view.worldRect.size.y <= 0.0f ||
        !std::isfinite(view.worldRect.position.x) ||
        !std::isfinite(view.worldRect.position.y) ||
        !std::isfinite(view.rotation.asRadians()) ||
        !std::isfinite(options.time) || !std::isfinite(options.lightingScale) ||
        options.lightingScale <= 0.0f) {
        throw std::invalid_argument(
            "Map render scene and view must have finite positive dimensions");
    }
}

std::vector<const MapRenderer::Region*> MapRenderer::Impl::orderedRegions(
    const Scene& scene) {
    std::vector<const Region*> result;
    for (const Region& region : scene.regions) {
        if (region.tilemap) {
            result.push_back(&region);
        }
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const Region* left, const Region* right) {
                         return left->priority < right->priority;
                     });
    return result;
}

bool MapRenderer::Impl::layerVisible(const Scene& scene, const Sprite& sprite) {
    if (!sprite.visible) {
        return false;
    }
    for (const Region& region : scene.regions) {
        if ((sprite.regionId.empty() || region.id == sprite.regionId) &&
            region.tilemap) {
            const std::shared_ptr<TileLayer> layer =
                region.tilemap->getLayer(sprite.layer);
            if (layer && layer->getVisible()) {
                return true;
            }
        }
    }
    return false;
}
