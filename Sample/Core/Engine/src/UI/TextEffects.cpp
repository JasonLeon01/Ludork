#include <UI/TextEffects.hpp>

#include <Runtime/EngineState.hpp>
#include <UI/Text.hpp>
#include <UI/TextEffectResources.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ludork::engine::text_effects {
namespace {

constexpr std::size_t CurveSampleCount = 256;

bool glowEnabled(const TextGlowConfig& glow) {
    return glow.enabled && glow.radius > 0.0f && glow.intensity > 0.0f &&
           glow.color.a > 0;
}

float glowRadiusPixels(const TextGlowConfig& glow) {
    return glowEnabled(glow) ? std::max(0.0f, glow.radius * Scale) : 0.0f;
}

std::uint8_t curveTextureChannel(float value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0l, 255l));
}

std::unique_ptr<sf::Texture> buildCurveTexture(
    const std::shared_ptr<Vector4Curve>& curve) {
    sf::Image image({static_cast<unsigned int>(CurveSampleCount), 1u},
                    sf::Color::White);
    for (std::size_t index = 0; index < CurveSampleCount; ++index) {
        const float input = static_cast<float>(index) /
                            static_cast<float>(CurveSampleCount - 1);
        const std::array<float, 4> output =
            curve == nullptr
                ? std::array<float, 4>{255.0f, 255.0f, 255.0f, 255.0f}
                : curve->evaluate(input);
        image.setPixel({static_cast<unsigned int>(index), 0u},
                       sf::Color(curveTextureChannel(output[0]),
                                 curveTextureChannel(output[1]),
                                 curveTextureChannel(output[2]),
                                 curveTextureChannel(output[3])));
    }
    std::unique_ptr<sf::Texture> texture = std::make_unique<sf::Texture>(image);
    texture->setSmooth(true);
    return texture;
}

sf::FloatRect combinedTextBounds(const std::vector<Source>& sources,
                                 bool includeOutline) {
    bool hasBounds = false;
    float minimumX = 0.0f;
    float minimumY = 0.0f;
    float maximumX = 0.0f;
    float maximumY = 0.0f;
    for (const Source& source : sources) {
        if (source.text == nullptr) {
            continue;
        }
        sf::Text measuredText = *source.text;
        if (!includeOutline) {
            measuredText.setOutlineThickness(0.0f);
        }
        const sf::FloatRect bounds =
            source.transform.transformRect(measuredText.getGlobalBounds());
        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
            continue;
        }
        const float right = bounds.position.x + bounds.size.x;
        const float bottom = bounds.position.y + bounds.size.y;
        if (!hasBounds) {
            minimumX = bounds.position.x;
            minimumY = bounds.position.y;
            maximumX = right;
            maximumY = bottom;
            hasBounds = true;
            continue;
        }
        minimumX = std::min(minimumX, bounds.position.x);
        minimumY = std::min(minimumY, bounds.position.y);
        maximumX = std::max(maximumX, right);
        maximumY = std::max(maximumY, bottom);
    }
    if (!hasBounds) {
        return {};
    }
    return {{minimumX, minimumY}, {maximumX - minimumX, maximumY - minimumY}};
}

void clear(Cache& cache) {
    cache.fill.reset();
    cache.outline.reset();
    cache.curve.reset();
    cache.sprite.reset();
    cache.shader.reset();
    cache.pixelBounds = {};
    cache.contentMinimum = {};
    cache.contentMaximum = {};
    cache.dirty = false;
}

}  // namespace

Cache::Cache() = default;

Cache::~Cache() = default;

bool enabled(const TextGlowConfig& glow,
             const TextGradientConfig& gradient) {
    return glowEnabled(glow) || gradient.enabled;
}

sf::FloatRect expandedBounds(const sf::FloatRect& bounds,
                             const TextGlowConfig& glow) {
    const float radius = glowRadiusPixels(glow);
    return {{bounds.position.x - radius, bounds.position.y - radius},
            {bounds.size.x + radius * 2.0f, bounds.size.y + radius * 2.0f}};
}

void rebuild(Cache& cache, const std::vector<Source>& sources,
             const TextGlowConfig& glow,
             const TextGradientConfig& gradient) {
    const sf::FloatRect renderBounds = combinedTextBounds(sources, true);
    const sf::FloatRect fillBounds = combinedTextBounds(sources, false);
    if (renderBounds.size.x <= 0.0f || renderBounds.size.y <= 0.0f ||
        fillBounds.size.x <= 0.0f || fillBounds.size.y <= 0.0f) {
        clear(cache);
        return;
    }

    const std::shared_ptr<sf::Shader> loadedShader = loadShader();
    if (loadedShader == nullptr) {
        clear(cache);
        return;
    }

    const float padding = std::ceil(glowRadiusPixels(glow)) + 2.0f;
    const float left = std::floor(renderBounds.position.x - padding);
    const float top = std::floor(renderBounds.position.y - padding);
    const float right =
        std::ceil(renderBounds.position.x + renderBounds.size.x + padding);
    const float bottom =
        std::ceil(renderBounds.position.y + renderBounds.size.y + padding);
    const sf::Vector2u textureSize{
        static_cast<unsigned int>(std::max(1.0f, right - left)),
        static_cast<unsigned int>(std::max(1.0f, bottom - top)),
    };
    const unsigned int maximumSize = sf::Texture::getMaximumSize();
    if (textureSize.x > maximumSize || textureSize.y > maximumSize) {
        warnOnce(
            "Text.effectCacheMaximum",
            "Text effects were skipped because the cache exceeds the maximum "
            "texture size");
        clear(cache);
        return;
    }

    cache.pixelBounds = {
        {left, top},
        {static_cast<float>(textureSize.x), static_cast<float>(textureSize.y)}};
    cache.contentMinimum = {
        (fillBounds.position.x - left) / static_cast<float>(textureSize.x),
        (fillBounds.position.y - top) / static_cast<float>(textureSize.y),
    };
    cache.contentMaximum = {
        (fillBounds.position.x + fillBounds.size.x - left) /
            static_cast<float>(textureSize.x),
        (fillBounds.position.y + fillBounds.size.y - top) /
            static_cast<float>(textureSize.y),
    };

    cache.fill = std::make_unique<sf::RenderTexture>(textureSize);
    cache.outline = std::make_unique<sf::RenderTexture>(textureSize);
    cache.fill->clear(sf::Color::Transparent);
    cache.outline->clear(sf::Color::Transparent);
    sf::RenderStates cacheStates;
    cacheStates.transform.translate({-left, -top});

    for (const Source& source : sources) {
        if (source.text == nullptr) {
            continue;
        }
        sf::RenderStates sourceStates = cacheStates;
        sourceStates.transform.combine(source.transform);
        sf::Text fillText = *source.text;
        fillText.setFillColor(source.fillColor);
        fillText.setOutlineThickness(0.0f);
        cache.fill->draw(fillText, sourceStates);

        if (source.text->getOutlineThickness() != 0.0f &&
            source.outlineColor.a != 0) {
            sf::Text outlineText = *source.text;
            outlineText.setFillColor(sf::Color::Transparent);
            outlineText.setOutlineColor(source.outlineColor);
            cache.outline->draw(outlineText, sourceStates);
        }
    }

    cache.fill->display();
    cache.outline->display();
    cache.curve = buildCurveTexture(gradient.curve);
    cache.sprite = std::make_unique<sf::Sprite>(cache.fill->getTexture());
    cache.sprite->setPosition(cache.pixelBounds.position);
    cache.shader = loadedShader;
    cache.dirty = false;
}

bool draw(Cache& cache, sf::RenderTarget& target, sf::RenderStates states,
          const sf::Color& colour, const TextGlowConfig& glow,
          const TextGradientConfig& gradient) {
    if (cache.sprite == nullptr || cache.shader == nullptr ||
        cache.fill == nullptr || cache.outline == nullptr ||
        cache.curve == nullptr) {
        return false;
    }

    cache.sprite->setColor(colour);
    cache.shader->setUniform("texture", cache.fill->getTexture());
    cache.shader->setUniform("outlineTexture", cache.outline->getTexture());
    cache.shader->setUniform("curveTexture", *cache.curve);
    cache.shader->setUniform("textureSize", cache.pixelBounds.size);
    cache.shader->setUniform("contentMinimum", cache.contentMinimum);
    cache.shader->setUniform("contentMaximum", cache.contentMaximum);
    cache.shader->setUniform("gradientEnabled", gradient.enabled);
    cache.shader->setUniform("gradientDirection",
                             gradient.direction == "horizontal" ? 1 : 0);
    cache.shader->setUniform("glowEnabled", glowEnabled(glow));
    cache.shader->setUniform("glowColor", sf::Glsl::Vec4(glow.color));
    cache.shader->setUniform("glowRadius", glowRadiusPixels(glow));
    cache.shader->setUniform("glowIntensity",
                             std::clamp(glow.intensity, 0.0f, 1.0f));
    states.shader = cache.shader.get();
    target.draw(*cache.sprite, states);
    return true;
}

}  // namespace ludork::engine::text_effects
