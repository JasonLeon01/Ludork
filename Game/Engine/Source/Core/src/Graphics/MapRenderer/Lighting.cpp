#include "MapRendererImpl.hpp"

#include <EngineState.hpp>
#include <LudorkGenerated/LightingConstants.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

void MapRenderer::Impl::renderLighting(const Scene& scene, const View& view,
                                       const Options& options) {
    const sf::Vector2u lightSize(
        static_cast<unsigned int>(std::max(
            1.0f, std::ceil(view.pixelSize.x * options.lightingScale))),
        static_cast<unsigned int>(std::max(
            1.0f, std::ceil(view.pixelSize.y * options.lightingScale))));
    sf::RenderTexture& direct = target(directLight_, lightSize);
    direct.setSmooth(options.lightingScale < 1.0f);
    direct.setView(nativeView(view));
    direct.clear(sf::Color::Black);
    if (options.lighting != LightingMode::Lit) {
        direct.display();
        return;
    }
    const sf::Vector2f centre =
        view.worldRect.position + view.worldRect.size * 0.5f;
    const float viewRadius = view.worldRect.size.length() * 0.5f;
    std::vector<Light> lights;
    for (const Light& light : scene.lights) {
        if (std::isfinite(light.position.x) &&
            std::isfinite(light.position.y) && std::isfinite(light.radius) &&
            std::isfinite(light.intensity) && light.radius > 0.0f &&
            light.intensity > 0.0f &&
            (light.position - centre).lengthSquared() <
                (light.radius + viewRadius) * (light.radius + viewRadius)) {
            lights.push_back(light);
        }
    }
    std::stable_sort(lights.begin(), lights.end(),
                     [&centre](const Light& left, const Light& right) {
                         return (left.position - centre).lengthSquared() <
                                (right.position - centre).lengthSquared();
                     });
    if (lights.size() > static_cast<std::size_t>(
                            ludork::generated::lighting::MaximumShaderLights)) {
        lights.resize(ludork::generated::lighting::MaximumShaderLights);
    }
    if (lights.empty()) {
        direct.display();
        return;
    }
    struct Occluder {
        const Sprite* sprite;
        sf::FloatRect bounds;
    };
    std::vector<Occluder> occluders;
    occluders.reserve(scene.sprites.size());
    for (const Sprite& sprite : scene.sprites) {
        if (!sprite.texture || !sprite.affectsLighting ||
            !layerVisible(scene, sprite)) {
            continue;
        }
        const auto resource =
            sprite.materialResource
                ? sprite.materialResource
                : ludork::engine::materials::MaterialResource::load(
                      sprite.material);
        if (!ludork::engine::materials::MaterialCompiler::analyse(
                 *resource, sprite.materialParameters)
                 .mayBlockLight) {
            continue;
        }
        const sf::Vector2f displacement = resource->maximumVertexOffset();
        occluders.push_back(
            {&sprite, sprite.transform.transformRect(
                          {-displacement,
                           sf::Vector2f(std::abs(sprite.textureRect.size.x),
                                        std::abs(sprite.textureRect.size.y)) +
                               displacement * 2.0f})});
    }
    renderTransmission(scene, view, lights, options);
    lightCaches_.resize(lights.size());
    const std::shared_ptr<sf::Shader> pass =
        shader("/Game/Assets/Shaders/Global/LightPass.frag");
    const float cell = static_cast<float>(EngineState::CellSize);
    const float pixelDensity =
        std::min(
            {1.0f, static_cast<float>(view.pixelSize.x) / view.worldRect.size.x,
             static_cast<float>(view.pixelSize.y) / view.worldRect.size.y}) *
        options.lightingScale;
    pass->setUniform("staticTransmission", transmission_->getTexture());
    pass->setUniform("staticOccupancy", *occupancy_);
    pass->setUniform("staticViewMode", 1.0f);
    pass->setUniform("staticTextureOrigin", transmissionRect_.position);
    pass->setUniform("staticTextureSize", transmissionRect_.size);
    pass->setUniform("staticOccupancyOrigin",
                     transmissionRect_.position / cell);
    pass->setUniform("staticOccupancySize", transmissionRect_.size / cell);
    pass->setUniform("mapViewOffset", sf::Vector2f{});
    sf::RenderStates states(sf::BlendAdd);
    states.shader = pass.get();
    for (std::size_t index = 0; index < lights.size(); ++index) {
        const Light& light = lights[index];
        const float diameter = light.radius * 2.0f;
        const float maxPixels =
            static_cast<float>(sf::Texture::getMaximumSize());
        const unsigned int cachePixels = static_cast<unsigned int>(
            std::clamp(std::ceil(diameter * pixelDensity), 1.0f, maxPixels));
        const sf::Vector2u cacheSize(cachePixels, cachePixels);
        const sf::FloatRect lightRect(
            light.position - sf::Vector2f(light.radius, light.radius),
            {diameter, diameter});
        View lightView{lightRect, sf::degrees(0.0f), cacheSize};
        sf::RectangleShape quad({diameter, diameter});
        quad.setPosition(lightRect.position);
        pass->setUniform("lightPos", light.position);
        pass->setUniform("lightColor", colour(light.colour, false));
        pass->setUniform("lightRadius", light.radius);
        pass->setUniform("lightIntensity", light.intensity);
        StaticLightCache& cache = lightCaches_[index];
        const bool changed = !cache.texture ||
                             cache.texture->getSize() != cacheSize ||
                             cache.generation != transmissionGeneration_ ||
                             cache.light.position != light.position ||
                             cache.light.colour != light.colour ||
                             cache.light.radius != light.radius ||
                             cache.light.intensity != light.intensity;
        if (changed) {
            sf::RenderTexture& buffer = target(cache.texture, cacheSize);
            buffer.setView(nativeView(lightView));
            buffer.clear(sf::Color::Black);
            setViewUniforms(*pass, scene, lightView);
            pass->setUniform("targetPixelScale",
                             sf::Vector2f(cacheSize) / diameter);
            pass->setUniform("useCachedStaticLight", 0.0f);
            pass->setUniform("traceStatic", 1.0f);
            pass->setUniform("traceDynamic", 0.0f);
            buffer.draw(quad, states);
            buffer.display();
            cache.light = light;
            cache.generation = transmissionGeneration_;
        }
        const bool hasOccluders = std::any_of(
            occluders.begin(), occluders.end(),
            [&light, &lightRect](const Occluder& occluder) {
                return (!light.emitterId ||
                        *light.emitterId != occluder.sprite->id) &&
                       occluder.bounds.findIntersection(lightRect).has_value();
            });
        const sf::Vector2i occupancyOrigin(
            static_cast<int>(std::floor(lightRect.position.x / cell)),
            static_cast<int>(std::floor(lightRect.position.y / cell)));
        const sf::Vector2u occupancySize(
            static_cast<unsigned int>(std::max(
                1.0f, std::ceil((lightRect.position.x + diameter) / cell) -
                          occupancyOrigin.x)),
            static_cast<unsigned int>(std::max(
                1.0f, std::ceil((lightRect.position.y + diameter) / cell) -
                          occupancyOrigin.y)));
        if (hasOccluders) {
            sf::RenderTexture& dynamic =
                target(dynamicTransmission_, cacheSize);
            dynamic.setView(nativeView(lightView));
            dynamic.clear(sf::Color::White);
            sf::Image occupancy(occupancySize, sf::Color::Black);
            for (const Occluder& occluder : occluders) {
                const Sprite& sprite = *occluder.sprite;
                if (light.emitterId && *light.emitterId == sprite.id) {
                    continue;
                }
                const sf::FloatRect& bounds = occluder.bounds;
                if (!bounds.findIntersection(lightRect)) {
                    continue;
                }
                drawSpriteMask(sprite, dynamic, true);
                const int left = std::max(
                    0, static_cast<int>(std::floor(bounds.position.x / cell)) -
                           occupancyOrigin.x);
                const int top = std::max(
                    0, static_cast<int>(std::floor(bounds.position.y / cell)) -
                           occupancyOrigin.y);
                const int right = std::min(
                    static_cast<int>(occupancySize.x),
                    static_cast<int>(
                        std::ceil((bounds.position.x + bounds.size.x) / cell)) -
                        occupancyOrigin.x);
                const int bottom = std::min(
                    static_cast<int>(occupancySize.y),
                    static_cast<int>(
                        std::ceil((bounds.position.y + bounds.size.y) / cell)) -
                        occupancyOrigin.y);
                for (int y = top; y < bottom; ++y) {
                    for (int x = left; x < right; ++x) {
                        occupancy.setPixel({static_cast<unsigned int>(x),
                                            occupancySize.y - 1 -
                                                static_cast<unsigned int>(y)},
                                           sf::Color::White);
                    }
                }
            }
            dynamic.display();
            if (!dynamicOccupancy_ ||
                dynamicOccupancy_->getSize() != occupancySize) {
                dynamicOccupancy_ = std::make_shared<sf::Texture>(occupancy);
            } else {
                dynamicOccupancy_->update(occupancy);
            }
        }
        setViewUniforms(*pass, scene, view);
        pass->setUniform(
            "targetPixelScale",
            sf::Vector2f(lightSize).componentWiseDiv(view.worldRect.size));
        pass->setUniform("cachedStaticLight", cache.texture->getTexture());
        pass->setUniform("useCachedStaticLight", 1.0f);
        pass->setUniform("traceStatic", 0.0f);
        pass->setUniform("traceDynamic", hasOccluders ? 1.0f : 0.0f);
        pass->setUniform("dynamicTransmission",
                         hasOccluders ? dynamicTransmission_->getTexture()
                                      : transmission_->getTexture());
        pass->setUniform("dynamicMaskOrigin", lightRect.position);
        pass->setUniform("dynamicMaskSize", lightRect.size);
        pass->setUniform("dynamicOccupancy",
                         hasOccluders ? *dynamicOccupancy_ : *occupancy_);
        pass->setUniform("dynamicOccupancyOrigin",
                         sf::Vector2f(occupancyOrigin));
        pass->setUniform("dynamicOccupancySize", sf::Vector2f(occupancySize));
        direct.draw(quad, states);
    }
    direct.display();
}
