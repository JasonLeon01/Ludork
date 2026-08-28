#include "GameMapBase.hpp"
#include "GameMapActorRegistry.hpp"

#include <Gameplay/Actor.hpp>
#include <Gameplay/TileMap.hpp>
#include <Runtime/EngineState.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

static float materialValueToFloat(const MaterialValue& value) {
    return std::visit(
        [](const auto& item) -> float {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, bool>) {
                return item ? 1.0f : 0.0f;
            } else {
                return item;
            }
        },
        value);
}

GameMapBase::GameMapBase()
    : actorRegistry_(std::make_unique<GameMapActorRegistry>()) {}

GameMapBase::~GameMapBase() = default;

void GameMapBase::setTilemap(std::shared_ptr<Tilemap> tilemap) {
    tilemap_ = std::move(tilemap);
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

std::shared_ptr<sf::Texture> GameMapBase::generateDataFromMap(
    const sf::Vector2u& size,
    const std::vector<std::vector<MaterialValue>>& materialMap, bool smooth) {
    int dataLen = size.x * size.y * 4;
    std::vector<std::uint8_t> pixelData(dataLen);
    for (int y = 0; y < size.y; ++y) {
        for (int x = 0; x < size.x; ++x) {
            int index = (y * size.x + x) * 4;
            pixelData[index] =
                std::uint8_t(materialValueToFloat(materialMap[y][x]) * 255.0f);
            pixelData[index + 1] = pixelData[index];
            pixelData[index + 2] = pixelData[index];
            pixelData[index + 3] = 255;
        }
    }

    sf::Image img(size, pixelData.data());
    std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromImage(img)) {
        throw std::runtime_error(
            "Failed to load texture from image at method generateDataFromMap");
    }
    texture->setSmooth(smooth);
    return texture;
}

std::vector<std::vector<MaterialValue>> GameMapBase::getMaterialPropertyMapExt(
    int width, int height, const std::string& propertyName,
    const MaterialValue& invalidValue) {
    std::vector<std::vector<MaterialValue>> materialPropertyMap;
    materialPropertyMap.reserve(static_cast<std::size_t>(std::max(height, 0)));
    for (int y = 0; y < height; ++y) {
        materialPropertyMap.push_back({});
        materialPropertyMap.back().reserve(
            static_cast<std::size_t>(std::max(width, 0)));
        for (int x = 0; x < width; ++x) {
            materialPropertyMap[y].push_back(getMaterialProperty(
                {x, height - y - 1}, propertyName, invalidValue));
        }
    }
    return materialPropertyMap;
}
