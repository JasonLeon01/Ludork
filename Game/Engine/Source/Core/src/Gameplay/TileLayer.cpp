#include <Gameplay/TileLayer.hpp>

#include <EngineState.hpp>
#include <Utils/Inner.hpp>

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
      material(data.material),
      data_(data),
      width_(layerWidth(data)),
      height_(layerHeight(data)),
      texture_(std::move(texture)),
      autoTileTextures_(normalizeAutoTileTextures(data, autoTileTextures)),
      autoTileFrameCounts_(normalizeFrameCounts(data, autoTileFrameCounts)) {
    getLayerMaterial();
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
    TileLayerData result = data_;
    result.material = material;
    return result;
}

void TileLayer::writeBlock(int x, int y, const TileGrid& tileBlock,
                           const AutoTileGrid& autoTileBlock) {
    data_.validateBlock(x, y, tileBlock, autoTileBlock);
    writePendingBlock(x, y, tileBlock, autoTileBlock);
    data_.writeBlock(x, y, tileBlock, autoTileBlock);
    invalidateContent();
}

void TileLayer::updateBlock(int x, int y, const TileGrid& tileBlock,
                            const AutoTileGrid& autoTileBlock) {
    data_.validateBlock(x, y, tileBlock, autoTileBlock);
    updateGraphicsBlock(x, y, tileBlock, autoTileBlock);
    data_.writeBlock(x, y, tileBlock, autoTileBlock);
    invalidateContent();
}

void TileLayer::setAnimationTime(float seconds, float frameInterval) {
    TileLayerGraphics::setAnimationTime(seconds, frameInterval);
    materialTime_ = seconds;
}

void TileLayer::invalidateContent() {
    ++contentRevision_;
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
    materialTime_ = source.materialTime_;
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

std::shared_ptr<const ludork::engine::materials::MaterialResource>
TileLayer::getLayerMaterial() const {
    return ludork::engine::materials::MaterialResource::load(material);
}

void TileLayer::drawMaterialPass(
    sf::RenderTarget& target, sf::RenderStates states,
    ludork::engine::materials::MaterialProgramCache& programs,
    ludork::engine::materials::MaterialCompiler::Pass pass,
    ludork::engine::materials::MaterialProgramCache::Context context) const {
    context.time = materialTime_;
    const auto layer = getLayerMaterial();
    TileLayerGraphics::drawMaterial(target, states, programs, layer.get(), pass,
                                    context);
}

bool TileLayer::hasReflectiveMaterial(const sf::FloatRect& bounds) const {
    const auto layer = getLayerMaterial();
    return TileLayerGraphics::hasReflectiveMaterial(
        layer.get(), getInverseTransform().transformRect(bounds));
}

sf::Vector2f TileLayer::maximumMaterialOffset() const {
    return TileLayerGraphics::maximumMaterialOffset() +
           getLayerMaterial()->maximumVertexOffset();
}

bool TileLayer::hasAnimatedOcclusion() const {
    const auto layer = getLayerMaterial();
    return TileLayerGraphics::hasAnimatedOcclusion(layer.get());
}

void TileLayer::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    drawMaterialPass(target, states, materialPrograms_,
                     ludork::engine::materials::MaterialCompiler::Pass::Color,
                     {});
}

sf::Vector2u TileLayer::getGridSize() const {
    return {static_cast<unsigned int>(std::max(0, width_)),
            static_cast<unsigned int>(std::max(0, height_))};
}

bool TileLayer::isCellBuilt(const sf::Vector2i& position) const {
    return TileLayerGraphics::isCellBuilt(position);
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
