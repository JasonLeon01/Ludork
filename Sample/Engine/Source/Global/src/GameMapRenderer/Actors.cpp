#include "Impl.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr float HueEpsilon = 0.0001f;

float normaliseHue(float hue) {
    float result = std::fmod(hue, 360.0f);
    if (result < 0.0f) {
        result += 360.0f;
    }
    return result;
}

bool neutralHue(float hue) {
    return hue <= HueEpsilon || std::abs(hue - 360.0f) <= HueEpsilon;
}

}  // namespace

void GameMapRendererImpl::drawContent(
    sf::RenderTarget& target, const sf::RenderStates& states,
    bool applyPlayerCover, float shaderTime, int materialRevision,
    const std::function<void(const std::string&)>& drawLayerEffects) {
    const int playerLayer = applyPlayerCover ? playerLayerIndex() : -1;
    sf::Vector2i playerPosition;
    const bool refreshCover =
        applyPlayerCover &&
        preparePlayerCover(playerLayer, materialRevision, playerPosition);

    for (std::size_t index = 0; index < layerNames.size(); ++index) {
        const std::string& layerName = layerNames[index];
        const std::shared_ptr<TileLayer> layer = tilemap->getLayer(layerName);
        if (!layer || !layer->getVisible()) {
            continue;
        }
        if (refreshCover) {
            this->applyPlayerCover(*layer, static_cast<int>(index), playerLayer,
                                   playerPosition);
        }
        sf::RenderStates layerStates = states;
        if (const std::shared_ptr<sf::Shader> shader = layer->getShader()) {
            layerStates.shader = shader.get();
        }
        target.draw(*layer, layerStates);
        drawLayerActors(target, states, layerName, static_cast<int>(index),
                        playerLayer, applyPlayerCover, shaderTime);
        if (drawLayerEffects) {
            drawLayerEffects(layerName);
        }
    }
}

int GameMapRendererImpl::playerLayerIndex() const {
    const std::shared_ptr<Actor>& player = map.getPlayerActorForRenderer();
    if (!player) {
        return -1;
    }
    const ActorDict& actors = map.getMaterialActorsForRenderer();
    for (std::size_t index = 0; index < layerNames.size(); ++index) {
        const auto layerIt = actors.find(layerNames[index]);
        if (layerIt == actors.end()) {
            continue;
        }
        const auto playerIt =
            std::find_if(layerIt->second.begin(), layerIt->second.end(),
                         [&](const ActorPtr& actor) {
                             return actor.get() == player.get();
                         });
        if (playerIt != layerIt->second.end()) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool GameMapRendererImpl::preparePlayerCover(int layerIndex,
                                             int materialRevision,
                                             sf::Vector2i& playerPosition) {
    const std::shared_ptr<Actor>& player = map.getPlayerActorForRenderer();
    if (!player || layerIndex < 0) {
        resetTransparentTiles();
        return false;
    }
    playerPosition = player->getMapPosition();
    bool reusable = coverPlayerPosition == playerPosition &&
                    coverPlayerLayerIndex == layerIndex &&
                    coverMaterialRevision == materialRevision &&
                    coverLayers.size() == layerNames.size();
    if (reusable) {
        for (std::size_t index = 0; index < layerNames.size(); ++index) {
            const std::shared_ptr<TileLayer> layer =
                tilemap->getLayer(layerNames[index]);
            if (!layer || coverLayers[index].first != layer.get() ||
                coverLayers[index].second != layer->getVisible()) {
                reusable = false;
                break;
            }
        }
    }
    if (reusable) {
        return false;
    }

    resetTransparentTiles();
    coverPlayerPosition = playerPosition;
    coverPlayerLayerIndex = layerIndex;
    coverMaterialRevision = materialRevision;
    coverLayers.reserve(layerNames.size());
    for (const std::string& layerName : layerNames) {
        const std::shared_ptr<TileLayer> layer = tilemap->getLayer(layerName);
        if (!layer) {
            throw std::runtime_error("Game map layer is unavailable: " +
                                     layerName);
        }
        coverLayers.emplace_back(layer.get(), layer->getVisible());
    }
    return true;
}

void GameMapRendererImpl::resetTransparentTiles() {
    for (const TransparentTile& tile : transparentTiles) {
        if (tile.layer) {
            tile.layer->resetTileColor(tile.position.x, tile.position.y);
        }
    }
    transparentTiles.clear();
    coverLayers.clear();
    coverPlayerPosition.reset();
    coverPlayerLayerIndex = -1;
    coverMaterialRevision = -1;
}

void GameMapRendererImpl::applyPlayerCover(TileLayer& layer, int layerIndex,
                                           int playerLayerIndex,
                                           const sf::Vector2i& playerPosition) {
    if (layerIndex <= playerLayerIndex || playerLayerIndex < 0 ||
        !layer.hasContent(playerPosition)) {
        return;
    }
    const sf::Color colour{255, 255, 255, coverAlpha};
    for (const sf::Vector2i& position : layer.floodFillTransparent(
             playerPosition.x, playerPosition.y, colour)) {
        transparentTiles.push_back(
            {tilemap->getLayer(layer.getName()), position});
    }
}

void GameMapRendererImpl::drawLayerActors(sf::RenderTarget& target,
                                          const sf::RenderStates& states,
                                          const std::string& layerName,
                                          int layerIndex, int playerLayerIndex,
                                          bool applyPlayerCover,
                                          float shaderTime) {
    const ActorDict& actors = map.getMaterialActorsForRenderer();
    const auto layerIt = actors.find(layerName);
    if (layerIt == actors.end()) {
        return;
    }
    const std::shared_ptr<Actor>& player = map.getPlayerActorForRenderer();
    for (const std::shared_ptr<Actor>& actor : layerIt->second) {
        if (!actor || effectHiddenActors.contains(actor.get())) {
            continue;
        }
        int alpha = 255;
        if (applyPlayerCover && player && layerIndex > playerLayerIndex &&
            playerLayerIndex >= 0 && actor.get() != player.get() &&
            actor->intersects(*player)) {
            alpha = coverAlpha;
        }
        drawActor(target, states, actor, alpha, shaderTime);
    }
}

void GameMapRendererImpl::drawActor(sf::RenderTarget& target,
                                    const sf::RenderStates& states,
                                    const std::shared_ptr<Actor>& actor,
                                    int actorAlpha, float shaderTime) {
    if (!actor) {
        return;
    }
    const std::uint8_t alpha =
        static_cast<std::uint8_t>(std::clamp(actorAlpha, 0, 255));
    const float hue = normaliseHue(actor->hue);
    const bool hasHue = actorHueShader && !neutralHue(hue);
    if (actor->hasShaderError()) {
        actor->setColor({255, 0, 255, alpha});
        target.draw(*actor, states);
        return;
    }
    actor->setColor({255, 255, 255, alpha});
    const std::shared_ptr<sf::Shader> actorShader = actor->getShader();
    if (actorShader) {
        const std::shared_ptr<sf::Texture> texture = actor->getTexture();
        if (!texture) {
            throw std::runtime_error("Actor shader texture must not be nil");
        }
        const sf::Vector2u textureSize = texture->getSize();
        const sf::IntRect rect = actor->getTextureRect();
        actorShader->setUniform("texture", sf::Shader::CurrentTexture);
        actorShader->setUniform("time", shaderTime);
        actorShader->setUniform(
            "textureSize", sf::Vector2f{static_cast<float>(textureSize.x),
                                        static_cast<float>(textureSize.y)});
        actorShader->setUniform(
            "textureRect", sf::Glsl::Vec4(static_cast<float>(rect.position.x),
                                          static_cast<float>(rect.position.y),
                                          static_cast<float>(rect.size.x),
                                          static_cast<float>(rect.size.y)));
        if (hasHue &&
            drawActorShaderWithHue(target, *actor, *actorShader, hue, alpha)) {
            return;
        }
        sf::RenderStates actorStates;
        actorStates.shader = actorShader.get();
        target.draw(*actor, actorStates);
        return;
    }
    if (hasHue) {
        applyActorHueUniform(hue);
        sf::RenderStates hueStates = states;
        hueStates.shader = actorHueShader.get();
        target.draw(*actor, hueStates);
        return;
    }
    target.draw(*actor, states);
}

bool GameMapRendererImpl::drawActorShaderWithHue(sf::RenderTarget& target,
                                                 Actor& actor,
                                                 sf::Shader& actorShader,
                                                 float hue,
                                                 std::uint8_t actorAlpha) {
    if (!actorHueShader) {
        return false;
    }
    const std::shared_ptr<sf::Texture> texture = actor.getTexture();
    if (!texture) {
        return false;
    }
    const sf::IntRect rect = actor.getTextureRect();
    const sf::Vector2u size{
        static_cast<unsigned int>(
            std::max(1.0f, std::floor(static_cast<float>(rect.size.x)))),
        static_cast<unsigned int>(
            std::max(1.0f, std::floor(static_cast<float>(rect.size.y)))),
    };
    sf::RenderTexture& shaderBuffer = ensureActorShaderBuffer(size);
    sf::RenderTexture& hueBuffer = ensureActorHueBuffer(size);
    sf::Sprite localSprite(*texture, rect);
    localSprite.setColor({255, 255, 255, actorAlpha});
    sf::RenderStates shaderStates;
    shaderStates.shader = &actorShader;
    shaderBuffer.clear(sf::Color::Transparent);
    shaderBuffer.draw(localSprite, shaderStates);
    shaderBuffer.display();

    applyActorHueUniform(hue);
    sf::RenderStates hueStates;
    hueStates.shader = actorHueShader.get();
    sf::Sprite& sourceSprite =
        ensureActorHueSourceSprite(shaderBuffer.getTexture());
    sourceSprite.setTexture(shaderBuffer.getTexture(), true);
    sourceSprite.setColor(sf::Color::White);
    hueBuffer.clear(sf::Color::Transparent);
    hueBuffer.draw(sourceSprite, hueStates);
    hueBuffer.display();

    sf::Sprite resultSprite(hueBuffer.getTexture());
    sf::RenderStates resultStates;
    resultStates.transform.combine(actor.getTransform());
    target.draw(resultSprite, resultStates);
    return true;
}

sf::RenderTexture& GameMapRendererImpl::ensureActorShaderBuffer(
    const sf::Vector2u& size) {
    if (!actorShaderBuffer || actorShaderBuffer->getSize() != size) {
        actorShaderBuffer = std::make_shared<sf::RenderTexture>(size);
    }
    return *actorShaderBuffer;
}

sf::RenderTexture& GameMapRendererImpl::ensureActorHueBuffer(
    const sf::Vector2u& size) {
    if (!actorHueBuffer || actorHueBuffer->getSize() != size) {
        actorHueBuffer = std::make_shared<sf::RenderTexture>(size);
    }
    return *actorHueBuffer;
}

sf::Sprite& GameMapRendererImpl::ensureActorHueSourceSprite(
    const sf::Texture& texture) {
    if (!actorHueSourceSprite) {
        actorHueSourceSprite.emplace(texture);
    }
    return *actorHueSourceSprite;
}

void GameMapRendererImpl::applyActorHueUniform(float hue) {
    actorHueShader->setUniform("screenTex", sf::Shader::CurrentTexture);
    actorHueShader->setUniform("hue", hue);
}

void GameMapRendererImpl::setActorEffectHidden(
    const std::shared_ptr<Actor>& actor, bool hidden) {
    if (!actor) {
        return;
    }
    if (hidden) {
        effectHiddenActors.insert(actor.get());
    } else {
        effectHiddenActors.erase(actor.get());
    }
}
