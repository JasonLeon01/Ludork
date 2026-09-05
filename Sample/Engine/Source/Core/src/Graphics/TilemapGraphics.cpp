#include <Graphics/TilemapGraphics.hpp>
#include "TilemapGraphics/ChunkBuilder.hpp"
#include "TilemapGraphics/Pattern.hpp"
#include "TilemapGraphics/RenderRuntime.hpp"
#include <SFML/Graphics/PrimitiveType.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using ludork::engine::tilemap_graphics_impl::ChunkRange;
using ludork::engine::tilemap_graphics_impl::composeCellPattern;
using ludork::engine::tilemap_graphics_impl::TileChunk;

TileLayerGraphics::TileLayerGraphics(
    int width, int height, int tileSize, std::shared_ptr<sf::Texture> texture,
    const TileLayerData& data,
    const std::vector<std::shared_ptr<sf::Texture>>& autoTileTextures,
    const std::vector<int>& autoTileFrameCounts, bool deferred)
    : texture_(std::move(texture)),
      size_(static_cast<float>(width), static_cast<float>(height)),
      tileSize_(tileSize),
      tiles_(data.tiles),
      passable_(data.layerTileset.passable),
      materials_(data.layerTileset.materials),
      autoTiles_(data.autoTiles),
      autoTilePool_(data.autoTilePool),
      autoTileTextures_(autoTileTextures),
      autoTileFrameCounts_(autoTileFrameCounts),
      autoTileAnimationAccum_(0.0f) {
    autoTileMaterials_.reserve(autoTilePool_.size());
    for (const auto& autoTile : autoTilePool_) {
        autoTileMaterials_.push_back(autoTile.material);
    }
    autoTileCurrentFrames_.assign(autoTileTextures_.size(), 0);
    initChunks();
    if (deferred) {
        buildComplete_ = chunks_.empty();
    } else {
        buildAllChunks();
    }
}

TileLayerGraphics::~TileLayerGraphics() = default;

void TileLayerGraphics::initChunks() {
    const int width = static_cast<int>(size_.x);
    const int height = static_cast<int>(size_.y);
    chunkColumns_ = width > 0 ? (width + ChunkSize - 1) / ChunkSize : 0;
    chunkRows_ = height > 0 ? (height + ChunkSize - 1) / ChunkSize : 0;
    chunks_ = ludork::engine::tilemap_graphics_impl::createChunks(width, height,
                                                                  ChunkSize);
    builtChunks_.assign(chunks_.size(), false);
}

TileChunk& TileLayerGraphics::getChunk(int x, int y) {
    return chunks_[static_cast<std::size_t>((y / ChunkSize) * chunkColumns_ +
                                            x / ChunkSize)];
}

void TileLayerGraphics::setTileColor(int x, int y, sf::Color color) {
    int width = static_cast<int>(size_.x);
    if (x < 0 || x >= width || y < 0 || y >= static_cast<int>(size_.y)) {
        return;
    }

    if (!tiles_[y][x].has_value()) {
        return;
    }

    TileChunk& chunk = getChunk(x, y);
    if (chunk.vertexArray == nullptr) {
        return;
    }
    int localX = x - chunk.x;
    int localY = y - chunk.y;
    int start = (localX + localY * chunk.width) * 6;
    for (int i = 0; i < 6; ++i) {
        (*chunk.vertexArray)[start + i].color = color;
    }
}

void TileLayerGraphics::resetTileColor(int x, int y) {
    int width = static_cast<int>(size_.x);
    if (x < 0 || x >= width || y < 0 || y >= static_cast<int>(size_.y)) {
        return;
    }

    if (!tiles_[y][x].has_value()) {
        return;
    }

    int tileNumber = tiles_[y][x].value();
    if (tileNumber < 0 || tileNumber >= static_cast<int>(materials_.size())) {
        return;
    }

    float opacity = materials_[tileNumber].opacity;
    sf::Color color = sf::Color::White;
    color.a = static_cast<std::uint8_t>(opacity * 255);

    setTileColor(x, y, color);
}

std::vector<sf::Vector2i> TileLayerGraphics::floodFillTransparent(
    int startX, int startY, sf::Color color) {
    std::vector<sf::Vector2i> processedTiles;
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);

    if (startX < 0 || startX >= width || startY < 0 || startY >= height) {
        return processedTiles;
    }

    if (!tiles_[startY][startX].has_value()) {
        return processedTiles;
    }

    std::vector<std::pair<int, int>> stack;
    stack.push_back({startX, startY});

    std::vector<bool> visited(width * height, false);
    visited[startY * width + startX] = true;

    while (!stack.empty()) {
        std::pair<int, int> current = stack.back();
        stack.pop_back();

        int cx = current.first;
        int cy = current.second;

        setTileColor(cx, cy, color);
        processedTiles.push_back({cx, cy});

        int dx[] = {0, 0, -1, 1};
        int dy[] = {-1, 1, 0, 0};

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int idx = ny * width + nx;
                if (tiles_[ny][nx].has_value() && !visited[idx]) {
                    visited[idx] = true;
                    stack.push_back({nx, ny});
                }
            }
        }
    }
    return processedTiles;
}

bool TileLayerGraphics::inBounds(const sf::Vector2i& position) const {
    return position.x >= 0 && position.y >= 0 &&
           position.x < static_cast<int>(size_.x) &&
           position.y < static_cast<int>(size_.y);
}

std::optional<int> TileLayerGraphics::getAutoTileIndex(
    const sf::Vector2i& position) const {
    if (!inBounds(position)) {
        return std::nullopt;
    }
    auto index = ludork::engine::tilemap_graphics_impl::autoTileIndexAt(
        autoTiles_, position.x, position.y);
    if (!index.has_value()) {
        return std::nullopt;
    }
    if (index.value() < 0 ||
        index.value() >= static_cast<int>(autoTilePool_.size())) {
        return std::nullopt;
    }
    return index;
}

std::optional<int> TileLayerGraphics::get(const sf::Vector2i& position) const {
    if (!inBounds(position)) {
        return std::nullopt;
    }
    if (position.y >= static_cast<int>(tiles_.size())) {
        return std::nullopt;
    }
    const auto& row = tiles_[position.y];
    if (position.x >= static_cast<int>(row.size())) {
        return std::nullopt;
    }
    return row[position.x];
}

std::optional<AutoTile> TileLayerGraphics::getAutoTileAt(
    const sf::Vector2i& position) const {
    auto index = getAutoTileIndex(position);
    if (!index.has_value()) {
        return std::nullopt;
    }
    return autoTilePool_[index.value()];
}

bool TileLayerGraphics::isPassable(const sf::Vector2i& position) const {
    if (!inBounds(position)) {
        return false;
    }
    auto autoTile = getAutoTileAt(position);
    if (autoTile.has_value()) {
        return autoTile.value().passable;
    }
    auto tileNumber = get(position);
    if (!tileNumber.has_value()) {
        return true;
    }
    int index = tileNumber.value();
    if (index < 0 || index >= static_cast<int>(passable_.size())) {
        return false;
    }
    return passable_[index];
}

std::optional<Material> TileLayerGraphics::getMaterial(
    const sf::Vector2i& position) const {
    if (!inBounds(position)) {
        return std::nullopt;
    }
    auto autoTile = getAutoTileAt(position);
    if (autoTile.has_value()) {
        return autoTile.value().material;
    }
    auto tileNumber = get(position);
    if (!tileNumber.has_value()) {
        return std::nullopt;
    }
    int index = tileNumber.value();
    if (index < 0 || index >= static_cast<int>(materials_.size())) {
        return std::nullopt;
    }
    return materials_[index];
}

std::vector<std::vector<float>> TileLayerGraphics::getLightBlockMap() const {
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);
    std::vector<std::vector<float>> result(height,
                                           std::vector<float>(width, 0.0f));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto material = getMaterial(sf::Vector2i(x, y));
            result[y][x] =
                material.has_value() ? material.value().lightBlock : 0.0f;
        }
    }
    return result;
}

std::vector<std::vector<float>> TileLayerGraphics::getReflectionStrengthMap()
    const {
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);
    std::vector<std::vector<float>> result(height,
                                           std::vector<float>(width, 0.0f));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto material = getMaterial(sf::Vector2i(x, y));
            if (material.has_value() && material.value().mirror) {
                result[y][x] = material.value().reflectionStrength;
            }
        }
    }
    return result;
}

std::vector<std::vector<float>> TileLayerGraphics::getIgnoreLightingMap()
    const {
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);
    std::vector<std::vector<float>> result(height,
                                           std::vector<float>(width, 0.0f));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto material = getMaterial(sf::Vector2i(x, y));
            result[y][x] =
                material.has_value() && material.value().ignoreLighting ? 1.0f
                                                                        : 0.0f;
        }
    }
    return result;
}

void TileLayerGraphics::updateAutoTileAnimation(float deltaTime,
                                                float frameInterval) {
    if (autoTileTextures_.empty() || frameInterval <= 0.0f) {
        return;
    }
    autoTileAnimationAccum_ += deltaTime;
    if (autoTileAnimationAccum_ < frameInterval) {
        return;
    }
    int steps = static_cast<int>(autoTileAnimationAccum_ / frameInterval);
    autoTileAnimationAccum_ -= steps * frameInterval;
    for (std::size_t i = 0; i < autoTileTextures_.size(); ++i) {
        int frameCount =
            i < autoTileFrameCounts_.size() ? autoTileFrameCounts_[i] : 1;
        if (frameCount <= 1) {
            continue;
        }
        int previous = autoTileCurrentFrames_[i];
        int next = (previous + steps) % frameCount;
        if (next != previous) {
            autoTileCurrentFrames_[i] = next;
            refreshAutoTileTexCoords(static_cast<int>(i));
        }
    }
}

int TileLayerGraphics::getLastVisibleChunkCount() const {
    return lastVisibleChunkCount_.load(std::memory_order_relaxed);
}

void TileLayerGraphics::draw(sf::RenderTarget& target,
                             sf::RenderStates states) const {
    lastVisibleChunkCount_.store(0, std::memory_order_relaxed);
    states.transform *= getTransform();
    if (chunkColumns_ == 0 || chunkRows_ == 0 || tileSize_ <= 0) {
        return;
    }
    const auto visible = ludork::engine::tilemap_graphics_impl::localViewBounds(
        target, states.transform);
    const ChunkRange range =
        ludork::engine::tilemap_graphics_impl::visibleChunkRange(
            visible, size_, tileSize_, ChunkSize, chunkColumns_, chunkRows_);
    if (range.lastX < range.firstX || range.lastY < range.firstY) {
        return;
    }

    int submittedChunks = 0;
    for (int chunkY = range.firstY; chunkY <= range.lastY; ++chunkY) {
        for (int chunkX = range.firstX; chunkX <= range.lastX; ++chunkX) {
            const TileChunk& chunk = chunks_[static_cast<std::size_t>(
                chunkY * chunkColumns_ + chunkX)];
            bool hasGeometry = chunk.vertexArray != nullptr;
            if (!hasGeometry) {
                for (const auto& autoTileVertexArray :
                     chunk.autoTileVertexArrays) {
                    if (autoTileVertexArray != nullptr) {
                        hasGeometry = true;
                        break;
                    }
                }
            }
            if (hasGeometry) {
                ++submittedChunks;
            }
        }
    }

    sf::RenderStates tileStates = states;
    tileStates.texture = texture_.get();
    for (int chunkY = range.firstY; chunkY <= range.lastY; ++chunkY) {
        for (int chunkX = range.firstX; chunkX <= range.lastX; ++chunkX) {
            const TileChunk& chunk = chunks_[static_cast<std::size_t>(
                chunkY * chunkColumns_ + chunkX)];
            if (chunk.vertexArray != nullptr) {
                target.draw(*chunk.vertexArray, tileStates);
            }
        }
    }
    for (std::size_t i = 0; i < autoTileTextures_.size(); ++i) {
        sf::RenderStates autoStates = states;
        autoStates.texture = autoTileTextures_[i].get();
        for (int chunkY = range.firstY; chunkY <= range.lastY; ++chunkY) {
            for (int chunkX = range.firstX; chunkX <= range.lastX; ++chunkX) {
                const TileChunk& chunk = chunks_[static_cast<std::size_t>(
                    chunkY * chunkColumns_ + chunkX)];
                if (i >= chunk.autoTileVertexArrays.size() ||
                    chunk.autoTileVertexArrays[i] == nullptr) {
                    continue;
                }
                target.draw(*chunk.autoTileVertexArrays[i], autoStates);
            }
        }
    }
    lastVisibleChunkCount_.store(submittedChunks, std::memory_order_relaxed);
}

void TileLayerGraphics::buildAllChunks() {
    while (!buildNextChunk()) {}
}

bool TileLayerGraphics::buildNextChunk() {
    if (buildComplete_) {
        return true;
    }
    while (nextBuildChunk_ < builtChunks_.size() &&
           builtChunks_[nextBuildChunk_]) {
        ++nextBuildChunk_;
    }
    if (nextBuildChunk_ >= chunks_.size()) {
        buildComplete_ = true;
        return true;
    }
    const int chunkX = static_cast<int>(
        nextBuildChunk_ % static_cast<std::size_t>(chunkColumns_));
    const int chunkY = static_cast<int>(
        nextBuildChunk_ / static_cast<std::size_t>(chunkColumns_));
    return buildChunk(chunkX, chunkY);
}

bool TileLayerGraphics::buildChunk(int chunkX, int chunkY) {
    if (chunkX < 0 || chunkX >= chunkColumns_ || chunkY < 0 ||
        chunkY >= chunkRows_) {
        throw std::out_of_range(
            "Tile layer chunk coordinates are out of range");
    }
    const std::size_t index =
        static_cast<std::size_t>(chunkY * chunkColumns_ + chunkX);
    if (builtChunks_[index]) {
        return buildComplete_;
    }
    TileChunk& chunk = chunks_[index];
    buildStaticChunk(chunk);
    buildAutoTileChunk(chunk);
    builtChunks_[index] = true;
    ++builtChunkCount_;
    while (nextBuildChunk_ < builtChunks_.size() &&
           builtChunks_[nextBuildChunk_]) {
        ++nextBuildChunk_;
    }
    buildComplete_ = builtChunkCount_ == chunks_.size();
    return buildComplete_;
}

bool TileLayerGraphics::isChunkBuilt(int chunkX, int chunkY) const {
    if (chunkX < 0 || chunkX >= chunkColumns_ || chunkY < 0 ||
        chunkY >= chunkRows_) {
        throw std::out_of_range(
            "Tile layer chunk coordinates are out of range");
    }
    const std::size_t index =
        static_cast<std::size_t>(chunkY * chunkColumns_ + chunkX);
    return builtChunks_[index];
}

bool TileLayerGraphics::isCellBuilt(const sf::Vector2i& position) const {
    if (!inBounds(position)) {
        return false;
    }
    return isChunkBuilt(position.x / ChunkSize, position.y / ChunkSize);
}

bool TileLayerGraphics::isBuildComplete() const {
    return buildComplete_;
}

void TileLayerGraphics::writePendingBlock(int x, int y,
                                          const TileGrid& tileBlock,
                                          const AutoTileGrid& autoTileBlock) {
    if (tileBlock.empty() || tileBlock.front().empty()) {
        return;
    }
    const int blockWidth = static_cast<int>(tileBlock.front().size());
    const int blockHeight = static_cast<int>(tileBlock.size());
    const int firstChunkX = x / ChunkSize;
    const int firstChunkY = y / ChunkSize;
    const int lastChunkX = (x + blockWidth - 1) / ChunkSize;
    const int lastChunkY = (y + blockHeight - 1) / ChunkSize;
    for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY) {
        for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX) {
            const std::size_t index =
                static_cast<std::size_t>(chunkY * chunkColumns_ + chunkX);
            if (builtChunks_[index]) {
                throw std::logic_error(
                    "Tile layer block overlaps an already built chunk");
            }
        }
    }
    const std::size_t destinationX = static_cast<std::size_t>(x);
    const std::size_t destinationY = static_cast<std::size_t>(y);
    for (std::size_t row = 0; row < tileBlock.size(); ++row) {
        std::copy(tileBlock[row].begin(), tileBlock[row].end(),
                  tiles_[destinationY + row].begin() + destinationX);
        std::copy(autoTileBlock[row].begin(), autoTileBlock[row].end(),
                  autoTiles_[destinationY + row].begin() + destinationX);
    }
}

void TileLayerGraphics::buildStaticChunk(TileChunk& chunk) {
    if (texture_ == nullptr || tileSize_ <= 0) {
        return;
    }
    const int columns = static_cast<int>(texture_->getSize().x) / tileSize_;
    if (columns <= 0) {
        return;
    }
    for (int y = chunk.y; y < chunk.y + chunk.height; ++y) {
        for (int x = chunk.x; x < chunk.x + chunk.width; ++x) {
            const std::optional<int>& tileNumberValue = tiles_[y][x];
            if (!tileNumberValue.has_value()) {
                continue;
            }
            const int tileNumber = *tileNumberValue;
            if (tileNumber < 0 ||
                tileNumber >= static_cast<int>(materials_.size())) {
                continue;
            }
            const float opacity = materials_[tileNumber].opacity;
            if (opacity <= 0.0f) {
                continue;
            }
            if (chunk.vertexArray == nullptr) {
                chunk.vertexArray = std::make_unique<sf::VertexArray>(
                    sf::PrimitiveType::Triangles,
                    static_cast<std::size_t>(chunk.width * chunk.height * 6));
            }
            const int textureX = tileNumber % columns;
            const int textureY = tileNumber / columns;
            const int localX = x - chunk.x;
            const int localY = y - chunk.y;
            const int start = (localX + localY * chunk.width) * 6;
            const std::array<sf::Vector2f, 6> positions = {
                sf::Vector2f(x * tileSize_, y * tileSize_),
                sf::Vector2f((x + 1) * tileSize_, y * tileSize_),
                sf::Vector2f(x * tileSize_, (y + 1) * tileSize_),
                sf::Vector2f(x * tileSize_, (y + 1) * tileSize_),
                sf::Vector2f((x + 1) * tileSize_, y * tileSize_),
                sf::Vector2f((x + 1) * tileSize_, (y + 1) * tileSize_),
            };
            const std::array<sf::Vector2f, 6> textureCoordinates = {
                sf::Vector2f(textureX * tileSize_, textureY * tileSize_),
                sf::Vector2f((textureX + 1) * tileSize_, textureY * tileSize_),
                sf::Vector2f(textureX * tileSize_, (textureY + 1) * tileSize_),
                sf::Vector2f(textureX * tileSize_, (textureY + 1) * tileSize_),
                sf::Vector2f((textureX + 1) * tileSize_, textureY * tileSize_),
                sf::Vector2f((textureX + 1) * tileSize_,
                             (textureY + 1) * tileSize_),
            };
            sf::Color colour = sf::Color::White;
            colour.a = static_cast<std::uint8_t>(opacity * 255.0f);
            for (int vertex = 0; vertex < 6; ++vertex) {
                (*chunk.vertexArray)[start + vertex].position =
                    positions[vertex];
                (*chunk.vertexArray)[start + vertex].texCoords =
                    textureCoordinates[vertex];
                if (opacity < 1.0f) {
                    (*chunk.vertexArray)[start + vertex].color = colour;
                }
            }
        }
    }
}

void TileLayerGraphics::buildAutoTileChunk(TileChunk& chunk) {
    const std::size_t poolSize = autoTileTextures_.size();
    chunk.autoTileVertexArrays.clear();
    chunk.autoTileVertexArrays.resize(poolSize);
    chunk.autoTileCells.assign(poolSize, {});
    chunk.autoTileMasks.assign(poolSize, {});
    if (poolSize == 0 || autoTiles_.empty()) {
        return;
    }
    for (int y = chunk.y; y < chunk.y + chunk.height; ++y) {
        for (int x = chunk.x; x < chunk.x + chunk.width; ++x) {
            const std::optional<int> poolIndexValue =
                getAutoTileIndex(sf::Vector2i(x, y));
            if (!poolIndexValue.has_value()) {
                continue;
            }
            const int poolIndex = *poolIndexValue;
            if (poolIndex < 0 || poolIndex >= static_cast<int>(poolSize)) {
                continue;
            }
            const int mask =
                ludork::engine::tilemap_graphics_impl::autoTileMask(
                    autoTiles_, x, y, poolIndex);
            chunk.autoTileCells[poolIndex].push_back({x, y});
            chunk.autoTileMasks[poolIndex].push_back(mask);
        }
    }
    for (std::size_t poolIndex = 0; poolIndex < poolSize; ++poolIndex) {
        const std::size_t cellCount = chunk.autoTileCells[poolIndex].size();
        if (cellCount == 0) {
            continue;
        }
        chunk.autoTileVertexArrays[poolIndex] =
            std::make_unique<sf::VertexArray>(sf::PrimitiveType::Triangles,
                                              cellCount * 4 * 6);
        for (std::size_t cell = 0; cell < cellCount; ++cell) {
            const int cellX = chunk.autoTileCells[poolIndex][cell].first;
            const int cellY = chunk.autoTileCells[poolIndex][cell].second;
            const int half = tileSize_ / 2;
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                const int quadrantX = quadrant % 2;
                const int quadrantY = quadrant / 2;
                const float left =
                    static_cast<float>(cellX * tileSize_ + quadrantX * half);
                const float top =
                    static_cast<float>(cellY * tileSize_ + quadrantY * half);
                const float right = left + static_cast<float>(half);
                const float bottom = top + static_cast<float>(half);
                const std::size_t base = (cell * 4 + quadrant) * 6;
                sf::VertexArray& vertices =
                    *chunk.autoTileVertexArrays[poolIndex];
                vertices[base + 0].position = sf::Vector2f(left, top);
                vertices[base + 1].position = sf::Vector2f(right, top);
                vertices[base + 2].position = sf::Vector2f(left, bottom);
                vertices[base + 3].position = sf::Vector2f(left, bottom);
                vertices[base + 4].position = sf::Vector2f(right, top);
                vertices[base + 5].position = sf::Vector2f(right, bottom);
            }
        }
        const float opacity = poolIndex < autoTileMaterials_.size()
                                  ? autoTileMaterials_[poolIndex].opacity
                                  : 1.0f;
        if (opacity < 1.0f) {
            sf::Color colour = sf::Color::White;
            colour.a = static_cast<std::uint8_t>(opacity * 255.0f);
            sf::VertexArray& vertices = *chunk.autoTileVertexArrays[poolIndex];
            for (std::size_t vertex = 0; vertex < vertices.getVertexCount();
                 ++vertex) {
                vertices[vertex].color = colour;
            }
        }
        refreshAutoTileTexCoords(chunk, static_cast<int>(poolIndex));
    }
}

void TileLayerGraphics::refreshAutoTileTexCoords(int poolIndex) {
    if (poolIndex < 0 ||
        poolIndex >= static_cast<int>(autoTileTextures_.size())) {
        return;
    }
    for (TileChunk& chunk : chunks_) {
        refreshAutoTileTexCoords(chunk, poolIndex);
    }
}

void TileLayerGraphics::refreshAutoTileTexCoords(TileChunk& chunk,
                                                 int poolIndex) {
    if (poolIndex < 0 ||
        poolIndex >= static_cast<int>(chunk.autoTileVertexArrays.size())) {
        return;
    }
    sf::VertexArray* vertices = chunk.autoTileVertexArrays[poolIndex].get();
    if (vertices == nullptr) {
        return;
    }
    const int half = tileSize_ / 2;
    int frameCount = poolIndex < static_cast<int>(autoTileFrameCounts_.size())
                         ? autoTileFrameCounts_[poolIndex]
                         : 1;
    if (frameCount <= 0) {
        frameCount = 1;
    }
    const int frame = autoTileCurrentFrames_[poolIndex] % frameCount;
    const int frameOffsetX = frame * 3 * tileSize_;
    const std::vector<int>& masks = chunk.autoTileMasks[poolIndex];
    for (std::size_t cell = 0; cell < masks.size(); ++cell) {
        const std::array<int, 4> pattern = composeCellPattern(masks[cell]);
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            const int patternCell = pattern[quadrant] - 1;
            const int column = patternCell % 3;
            const int row = patternCell / 3;
            const int quadrantX = quadrant % 2;
            const int quadrantY = quadrant / 2;
            const int sourceX =
                column * tileSize_ + quadrantX * half + frameOffsetX;
            const int sourceY = row * tileSize_ + quadrantY * half;
            const float left = static_cast<float>(sourceX);
            const float top = static_cast<float>(sourceY);
            const float right = left + static_cast<float>(half);
            const float bottom = top + static_cast<float>(half);
            const std::size_t base = (cell * 4 + quadrant) * 6;
            (*vertices)[base + 0].texCoords = sf::Vector2f(left, top);
            (*vertices)[base + 1].texCoords = sf::Vector2f(right, top);
            (*vertices)[base + 2].texCoords = sf::Vector2f(left, bottom);
            (*vertices)[base + 3].texCoords = sf::Vector2f(left, bottom);
            (*vertices)[base + 4].texCoords = sf::Vector2f(right, top);
            (*vertices)[base + 5].texCoords = sf::Vector2f(right, bottom);
        }
    }
}
