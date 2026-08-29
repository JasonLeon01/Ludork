#include <Graphics/TilemapGraphics.hpp>

#include <SFML/Graphics/PrimitiveType.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace {
constexpr int kMaskTop = 0x01;
constexpr int kMaskRight = 0x02;
constexpr int kMaskBottom = 0x04;
constexpr int kMaskLeft = 0x08;
constexpr int kMaskTopLeft = 0x10;
constexpr int kMaskTopRight = 0x20;
constexpr int kMaskBottomRight = 0x40;
constexpr int kMaskBottomLeft = 0x80;
constexpr int kInnerFillerCell = 3;

constexpr std::array<std::array<int, 4>, 16> kBasePattern = {{
    {{1, 1, 1, 1}},
    {{10, 12, 10, 12}},
    {{4, 4, 10, 10}},
    {{10, 10, 10, 10}},
    {{4, 6, 4, 6}},
    {{7, 9, 7, 9}},
    {{4, 4, 4, 4}},
    {{7, 7, 7, 7}},
    {{6, 6, 12, 12}},
    {{12, 12, 12, 12}},
    {{5, 5, 11, 11}},
    {{11, 11, 11, 11}},
    {{6, 6, 6, 6}},
    {{9, 9, 9, 9}},
    {{5, 5, 5, 5}},
    {{8, 8, 8, 8}},
}};

constexpr std::array<std::array<int, 3>, 4> kQuadBits = {{
    {{kMaskTop, kMaskLeft, kMaskTopLeft}},
    {{kMaskTop, kMaskRight, kMaskTopRight}},
    {{kMaskBottom, kMaskLeft, kMaskBottomLeft}},
    {{kMaskBottom, kMaskRight, kMaskBottomRight}},
}};

std::array<int, 4> composeCellPattern(int mask) {
    std::array<int, 4> result = kBasePattern[mask & 0x0F];
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
        const int firstOrthogonal = kQuadBits[quadrant][0];
        const int secondOrthogonal = kQuadBits[quadrant][1];
        const int diagonal = kQuadBits[quadrant][2];
        if ((mask & firstOrthogonal) && (mask & secondOrthogonal) &&
            !(mask & diagonal)) {
            result[quadrant] = kInnerFillerCell;
        }
    }
    return result;
}
}  // namespace

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
            const auto sameAt = [this, poolIndex](int cellX, int cellY) {
                const std::optional<int> other =
                    getAutoTileIndex(sf::Vector2i(cellX, cellY));
                return other.has_value() && *other == poolIndex;
            };
            int mask = 0;
            if (sameAt(x, y - 1)) {
                mask |= kMaskTop;
            }
            if (sameAt(x + 1, y)) {
                mask |= kMaskRight;
            }
            if (sameAt(x, y + 1)) {
                mask |= kMaskBottom;
            }
            if (sameAt(x - 1, y)) {
                mask |= kMaskLeft;
            }
            if (sameAt(x - 1, y - 1)) {
                mask |= kMaskTopLeft;
            }
            if (sameAt(x + 1, y - 1)) {
                mask |= kMaskTopRight;
            }
            if (sameAt(x + 1, y + 1)) {
                mask |= kMaskBottomRight;
            }
            if (sameAt(x - 1, y + 1)) {
                mask |= kMaskBottomLeft;
            }
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
