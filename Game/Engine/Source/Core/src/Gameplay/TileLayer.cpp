#include <Gameplay/TileLayer.hpp>

#include <EngineState.hpp>
#include <Utils/Inner.hpp>
#include <Utils/ShaderLoader.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

TileLayer::TileLayer(
    const TileLayerData& data, std::shared_ptr<sf::Texture> texture,
    const std::vector<std::shared_ptr<sf::Texture>>& autoTileTextures,
    const std::vector<int>& autoTileFrameCounts, bool visible, bool deferred)
    : TileLayerGraphics(layerWidth(data), layerHeight(data),
                        EngineState::CellSize, requireTexture(texture), data,
                        normalizeAutoTileTextures(data, autoTileTextures),
                        normalizeFrameCounts(data, autoTileFrameCounts),
                        deferred),
      visible(visible),
      shaderPath(data.shaderPath),
      data_(data),
      width_(layerWidth(data)),
      height_(layerHeight(data)),
      texture_(std::move(texture)),
      autoTileTextures_(normalizeAutoTileTextures(data, autoTileTextures)),
      autoTileFrameCounts_(normalizeFrameCounts(data, autoTileFrameCounts)) {
    loadShader();
}

TileLayer::~TileLayer() = default;

const std::string& TileLayer::getName() const {
    return data_.layerName;
}

const TileGrid& TileLayer::getTiles() const {
    return data_.tiles;
}

const AutoTileGrid& TileLayer::getAutoTiles() const {
    return data_.autoTiles;
}

const std::vector<AutoTile>& TileLayer::getAutoTilePool() const {
    return data_.autoTilePool;
}

std::optional<std::string> TileLayer::getAutoTileKey(int poolIndex) const {
    if (poolIndex < 0 ||
        poolIndex >= static_cast<int>(data_.autoTileKeys.size())) {
        return std::nullopt;
    }
    return data_.autoTileKeys[static_cast<std::size_t>(poolIndex)];
}

TileLayerData TileLayer::getData() const {
    return data_;
}

void TileLayer::writeBlock(int x, int y, const TileGrid& tileBlock,
                           const AutoTileGrid& autoTileBlock) {
    data_.validateBlock(x, y, tileBlock, autoTileBlock);
    writePendingBlock(x, y, tileBlock, autoTileBlock);
    data_.writeBlock(x, y, tileBlock, autoTileBlock);
    ++contentRevision_;
    lightBlockMapCache_.reset();
    reflectionStrengthMapCache_.reset();
    ignoreLightingMapCache_.reset();
    lightBlockImageCache_.reset();
    reflectionStrengthImageCache_.reset();
    ignoreLightingImageCache_.reset();
}

std::vector<std::shared_ptr<sf::Texture>> TileLayer::getAutoTileTextures()
    const {
    return autoTileTextures_;
}

std::vector<int> TileLayer::getAutoTileFrameCounts() const {
    return autoTileFrameCounts_;
}

std::shared_ptr<TileLayer> TileLayer::rebuild(
    const TileLayerData& data,
    const std::vector<std::shared_ptr<sf::Texture>>& autoTileTextures,
    const std::vector<int>& autoTileFrameCounts) const {
    return std::make_shared<TileLayer>(data, texture_, autoTileTextures,
                                       autoTileFrameCounts, visible);
}

std::size_t TileLayer::getContentRevision() const {
    return contentRevision_;
}

std::shared_ptr<TileLayer> TileLayer::createDisplayLayer(
    const std::vector<std::vector<sf::Vector2i>>& sources) const {
    if (sources.size() != static_cast<std::size_t>(height_)) {
        throw std::invalid_argument(
            "Display source grid height must match the layer");
    }
    TileLayerData displayData = data_;
    displayData.autoTiles.assign(height_, std::vector<AutoTileCell>(width_));
    for (int y = 0; y < height_; ++y) {
        if (sources[y].size() != static_cast<std::size_t>(width_)) {
            throw std::invalid_argument(
                "Display source grid width must match the layer");
        }
        for (int x = 0; x < width_; ++x) {
            const sf::Vector2i source = sources[y][x];
            if (source == sf::Vector2i{-1, -1}) {
                displayData.tiles[y][x] = std::nullopt;
                continue;
            }
            if (source.x < 0 || source.y < 0 || source.x >= width_ ||
                source.y >= height_) {
                throw std::invalid_argument(
                    "Display source cell is outside the layer");
            }
            displayData.tiles[y][x] = data_.tiles[source.y][source.x];
            if (source.y < static_cast<int>(data_.autoTiles.size()) &&
                source.x < static_cast<int>(data_.autoTiles[source.y].size())) {
                displayData.autoTiles[y][x] =
                    data_.autoTiles[source.y][source.x];
            }
        }
    }
    auto result = rebuild(displayData, autoTileTextures_, autoTileFrameCounts_);
    result->copyDisplayAutoTileMasks(*this, sources);
    result->syncDisplayAnimation(*this);
    return result;
}

void TileLayer::syncDisplayAnimation(const TileLayer& source) {
    copyAutoTileAnimation(source);
    shaderTime_ = source.shaderTime_;
}

bool TileLayer::getVisible() const {
    return visible;
}

void TileLayer::setVisible(bool value) {
    visible = value;
}

bool TileLayer::hasContent(const sf::Vector2i& position) const {
    return get(position).has_value() || getAutoTileAt(position).has_value();
}

bool TileLayer::isDirectionPassable(const sf::Vector2i& position,
                                    int directionIndex) const {
    const std::optional<int> tile = get(position);
    if (!tile.has_value()) {
        return true;
    }
    if (directionIndex < 0 || directionIndex >= 4) {
        return false;
    }
    const std::size_t tileIndex = static_cast<std::size_t>(*tile);
    if (tileIndex >= data_.layerTileset.dir4.size()) {
        return true;
    }
    const auto& directions = data_.layerTileset.dir4[tileIndex];
    if (directions.size() != 4) {
        return true;
    }
    return directions[static_cast<std::size_t>(directionIndex)];
}

std::optional<MaterialValue> TileLayer::getMaterialProperty(
    const sf::Vector2i& position, const std::string& propertyName) const {
    const std::optional<Material> material = getMaterial(position);
    if (!material.has_value()) {
        return std::nullopt;
    }

    std::string fieldName = propertyName;
    if (propertyName == "getLightBlock") {
        fieldName = "lightBlock";
    } else if (propertyName == "getMirror") {
        fieldName = "mirror";
    } else if (propertyName == "getReflectionStrength") {
        fieldName = "reflectionStrength";
    } else if (propertyName == "getIgnoreLighting") {
        fieldName = "ignoreLighting";
    } else if (propertyName == "getSpeedRate") {
        fieldName = "speedRate";
    }

    const MaterialData values = material->asDict();
    const auto iterator = values.find(fieldName);
    return iterator == values.end()
               ? std::nullopt
               : std::optional<MaterialValue>(iterator->second);
}

std::optional<float> TileLayer::getLightBlock(
    const sf::Vector2i& position) const {
    return materialFloat(getMaterialProperty(position, "lightBlock"));
}

std::optional<bool> TileLayer::getMirror(const sf::Vector2i& position) const {
    return materialBool(getMaterialProperty(position, "mirror"));
}

std::optional<float> TileLayer::getReflectionStrength(
    const sf::Vector2i& position) const {
    return materialFloat(getMaterialProperty(position, "reflectionStrength"));
}

std::optional<bool> TileLayer::getIgnoreLighting(
    const sf::Vector2i& position) const {
    return materialBool(getMaterialProperty(position, "ignoreLighting"));
}

std::optional<float> TileLayer::getSpeedRate(
    const sf::Vector2i& position) const {
    return materialFloat(getMaterialProperty(position, "speedRate"));
}

void TileLayer::updateShader(float deltaTime) {
    if (shader == nullptr || !shaderUsesTime_) {
        return;
    }
    shaderTime_ += deltaTime;
}

void TileLayer::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (shader != nullptr && shaderUsesTime_ && states.shader == shader.get()) {
        shader->setUniform("time", shaderTime_);
    }
    TileLayerGraphics::draw(target, states);
}

std::shared_ptr<sf::Shader> TileLayer::getShader() const {
    return shader;
}

std::shared_ptr<sf::Image> TileLayer::getLightBlockImage() {
    if (lightBlockImageCache_ == nullptr) {
        lightBlockImageCache_ = buildMaterialImage(getLightBlockMapView());
    }
    return lightBlockImageCache_;
}

std::shared_ptr<sf::Image> TileLayer::getReflectionStrengthImage() {
    if (reflectionStrengthImageCache_ == nullptr) {
        if (!reflectionStrengthMapCache_.has_value()) {
            reflectionStrengthMapCache_ =
                TileLayerGraphics::getReflectionStrengthMap();
        }
        reflectionStrengthImageCache_ =
            buildMaterialImage(*reflectionStrengthMapCache_);
    }
    return reflectionStrengthImageCache_;
}

std::shared_ptr<sf::Image> TileLayer::getIgnoreLightingImage() {
    if (ignoreLightingImageCache_ == nullptr) {
        if (!ignoreLightingMapCache_.has_value()) {
            ignoreLightingMapCache_ = TileLayerGraphics::getIgnoreLightingMap();
        }
        ignoreLightingImageCache_ =
            buildMaterialImage(*ignoreLightingMapCache_);
    }
    return ignoreLightingImageCache_;
}

sf::Vector2u TileLayer::getGridSize() const {
    return {static_cast<unsigned int>(std::max(0, width_)),
            static_cast<unsigned int>(std::max(0, height_))};
}

bool TileLayer::isCellBuilt(const sf::Vector2i& position) const {
    return TileLayerGraphics::isCellBuilt(position);
}

const std::vector<std::vector<float>>& TileLayer::getLightBlockMapView() {
    if (!lightBlockMapCache_.has_value()) {
        lightBlockMapCache_ = TileLayerGraphics::getLightBlockMap();
    }
    return *lightBlockMapCache_;
}

int TileLayer::layerWidth(const TileLayerData& data) {
    return data.tiles.empty() ? 0 : static_cast<int>(data.tiles.front().size());
}

int TileLayer::layerHeight(const TileLayerData& data) {
    return static_cast<int>(data.tiles.size());
}

const std::shared_ptr<sf::Texture>& TileLayer::requireTexture(
    const std::shared_ptr<sf::Texture>& texture) {
    if (texture == nullptr) {
        throw std::invalid_argument("TileLayer texture must not be null");
    }
    return texture;
}

std::vector<std::shared_ptr<sf::Texture>> TileLayer::normalizeAutoTileTextures(
    const TileLayerData& data,
    const std::vector<std::shared_ptr<sf::Texture>>& textures) {
    std::vector<std::shared_ptr<sf::Texture>> result = textures;
    result.resize(data.autoTilePool.size());
    return result;
}

std::vector<int> TileLayer::normalizeFrameCounts(
    const TileLayerData& data, const std::vector<int>& frameCounts) {
    std::vector<int> result = frameCounts;
    result.resize(data.autoTilePool.size(), 1);
    for (int& frameCount : result) {
        if (frameCount <= 0) {
            frameCount = 1;
        }
    }
    return result;
}

std::optional<float> TileLayer::materialFloat(
    const std::optional<MaterialValue>& value) {
    if (!value.has_value()) {
        return std::nullopt;
    }
    const float* number = std::get_if<float>(&*value);
    return number == nullptr ? std::nullopt : std::optional<float>(*number);
}

std::optional<bool> TileLayer::materialBool(
    const std::optional<MaterialValue>& value) {
    if (!value.has_value()) {
        return std::nullopt;
    }
    const bool* boolean = std::get_if<bool>(&*value);
    return boolean == nullptr ? std::nullopt : std::optional<bool>(*boolean);
}

void TileLayer::loadShader() {
    if (shaderPath.empty()) {
        return;
    }
    const std::optional<sf::Shader::Type> shaderType =
        ShaderLoader::inferType(shaderPath);
    if (!shaderType.has_value()) {
        std::cerr << "Unsupported tile layer shader extension: " << shaderPath
                  << '\n';
        return;
    }

    ShaderLoadResult result = ShaderLoader::load(shaderPath, *shaderType);
    if (!result) {
        std::cerr << result.error << '\n';
        return;
    }
    shader = std::move(result.shader);
    shaderUsesTime_ =
        result.source.find("uniform float time") != std::string::npos;
}

std::shared_ptr<sf::Image> TileLayer::buildMaterialImage(
    const std::vector<std::vector<float>>& values) const {
    std::shared_ptr<sf::Image> image =
        std::make_shared<sf::Image>(getGridSize());
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float value = y < static_cast<int>(values.size()) &&
                                        x < static_cast<int>(values[y].size())
                                    ? values[y][x]
                                    : 0.0f;
            const int channel = std::clamp(
                static_cast<int>(std::floor(value * 255.0f)), 0, 255);
            image->setPixel(
                {static_cast<unsigned int>(x), static_cast<unsigned int>(y)},
                sf::Color(channel, channel, channel));
        }
    }
    return image;
}
