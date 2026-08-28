#include <Graphics/TilemapGraphics.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace {
struct LocalViewBounds {
    float left;
    float top;
    float right;
    float bottom;
};

LocalViewBounds getLocalViewBounds(sf::RenderTarget& target,
                                   const sf::Transform& transform) {
    const sf::View& view = target.getView();
    const sf::FloatRect viewport = view.getViewport();
    const sf::Vector2u targetSize = target.getSize();
    const int left = static_cast<int>(
        std::floor(viewport.position.x * static_cast<float>(targetSize.x)));
    const int top = static_cast<int>(
        std::floor(viewport.position.y * static_cast<float>(targetSize.y)));
    const int right =
        static_cast<int>(std::ceil((viewport.position.x + viewport.size.x) *
                                   static_cast<float>(targetSize.x)));
    const int bottom =
        static_cast<int>(std::ceil((viewport.position.y + viewport.size.y) *
                                   static_cast<float>(targetSize.y)));
    const std::array<sf::Vector2i, 4> pixelCorners = {
        sf::Vector2i(left, top), sf::Vector2i(right, top),
        sf::Vector2i(left, bottom), sf::Vector2i(right, bottom)};
    const sf::Transform inverse = transform.getInverse();
    LocalViewBounds bounds{std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::lowest(),
                           std::numeric_limits<float>::lowest()};
    for (const sf::Vector2i& pixel : pixelCorners) {
        const sf::Vector2f world = target.mapPixelToCoords(pixel, view);
        const sf::Vector2f local = inverse.transformPoint(world);
        bounds.left = std::min(bounds.left, local.x);
        bounds.top = std::min(bounds.top, local.y);
        bounds.right = std::max(bounds.right, local.x);
        bounds.bottom = std::max(bounds.bottom, local.y);
    }
    return bounds;
}

std::optional<int> autoTileIndexAt(const AutoTileGrid& grid, int x, int y) {
    if (y < 0 || y >= static_cast<int>(grid.size())) {
        return std::nullopt;
    }
    const auto& row = grid[y];
    if (x < 0 || x >= static_cast<int>(row.size())) {
        return std::nullopt;
    }
    const auto& cell = row[x];
    if (!cell.has_value()) {
        return std::nullopt;
    }
    if (const auto index = std::get_if<int>(&cell.value())) {
        return *index;
    }
    return std::nullopt;
}

}  // namespace

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
    chunks_.clear();
    chunks_.reserve(static_cast<std::size_t>(chunkColumns_ * chunkRows_));
    for (int chunkY = 0; chunkY < chunkRows_; ++chunkY) {
        for (int chunkX = 0; chunkX < chunkColumns_; ++chunkX) {
            TileChunk chunk;
            chunk.x = chunkX * ChunkSize;
            chunk.y = chunkY * ChunkSize;
            chunk.width = std::min(ChunkSize, width - chunk.x);
            chunk.height = std::min(ChunkSize, height - chunk.y);
            chunks_.push_back(std::move(chunk));
        }
    }
    builtChunks_.assign(chunks_.size(), false);
}

TileLayerGraphics::TileChunk& TileLayerGraphics::getChunk(int x, int y) {
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
    auto index = autoTileIndexAt(autoTiles_, position.x, position.y);
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
    const LocalViewBounds visible =
        getLocalViewBounds(target, states.transform);
    const float layerWidth = size_.x * static_cast<float>(tileSize_);
    const float layerHeight = size_.y * static_cast<float>(tileSize_);
    if (visible.right <= 0.0f || visible.bottom <= 0.0f ||
        visible.left >= layerWidth || visible.top >= layerHeight) {
        return;
    }
    const float chunkPixelSize = static_cast<float>(ChunkSize * tileSize_);
    const int firstChunkX =
        std::clamp(static_cast<int>(std::floor(visible.left / chunkPixelSize)),
                   0, chunkColumns_ - 1);
    const int firstChunkY =
        std::clamp(static_cast<int>(std::floor(visible.top / chunkPixelSize)),
                   0, chunkRows_ - 1);
    const int lastChunkX = std::clamp(
        static_cast<int>(std::ceil(visible.right / chunkPixelSize)) - 1, 0,
        chunkColumns_ - 1);
    const int lastChunkY = std::clamp(
        static_cast<int>(std::ceil(visible.bottom / chunkPixelSize)) - 1, 0,
        chunkRows_ - 1);

    int submittedChunks = 0;
    for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY) {
        for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX) {
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
    for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY) {
        for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX) {
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
        for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY) {
            for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX) {
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
