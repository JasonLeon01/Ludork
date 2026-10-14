#include <Graphics/TilemapGraphics.hpp>
#include "TilemapGraphics/ChunkBuilder.hpp"
#include "TilemapGraphics/AutoTileAtlas.hpp"
#include <stdexcept>
#include "TilemapGraphics/Pattern.hpp"
#include "TilemapGraphics/RenderImpl.hpp"
#include <SFML/Graphics/PrimitiveType.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using ludork::engine::tilemap_graphics_impl::ChunkRange;
using ludork::engine::tilemap_graphics_impl::composeCellPattern;
using ludork::engine::tilemap_graphics_impl::TileChunk;

namespace {
sf::FloatRect combinedBounds(const sf::FloatRect& left,
                             const sf::FloatRect& right) {
    const sf::Vector2f minimum{std::min(left.position.x, right.position.x),
                               std::min(left.position.y, right.position.y)};
    const sf::Vector2f maximum{std::max(left.position.x + left.size.x,
                                        right.position.x + right.size.x),
                               std::max(left.position.y + left.size.y,
                                        right.position.y + right.size.y)};
    return {minimum, maximum - minimum};
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
    defaultMaterial_ =
        ludork::engine::materials::MaterialResource::defaultMaterial();
    for (const auto& [index, path] : materials_) {
        if (index < 0 || index >= getTilesetTileCount() || path.empty()) {
            throw std::invalid_argument(
                "Tileset material reference is outside the texture or empty");
        }
        const auto resource =
            ludork::engine::materials::MaterialResource::load(path);
        materialResources_.emplace(index, resource);
        maximumMaterialOffset_.x = std::max(maximumMaterialOffset_.x,
                                            resource->maximumVertexOffset().x);
        maximumMaterialOffset_.y = std::max(maximumMaterialOffset_.y,
                                            resource->maximumVertexOffset().y);
    }
    for (std::size_t index = 0; index < autoTilePool_.size(); ++index) {
        const auto resource = ludork::engine::materials::MaterialResource::load(
            autoTilePool_[index].material);
        autoTileMaterials_.push_back(resource);
        maximumMaterialOffset_.x = std::max(maximumMaterialOffset_.x,
                                            resource->maximumVertexOffset().x);
        maximumMaterialOffset_.y = std::max(maximumMaterialOffset_.y,
                                            resource->maximumVertexOffset().y);
        autoTileAtlases_.push_back(
            index < autoTileTextures_.size() && autoTileTextures_[index]
                ? std::make_shared<
                      ludork::engine::tilemap_graphics_impl::AutoTileAtlas>(
                      *autoTileTextures_[index], tileSize_,
                      index < autoTileFrameCounts_.size()
                          ? autoTileFrameCounts_[index]
                          : 1)
                : nullptr);
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
    return chunks_[ludork::engine::tilemap_graphics_impl::chunkIndex(
        x / ChunkSize, y / ChunkSize, chunkColumns_)];
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
    const int start =
        ludork::engine::tilemap_graphics_impl::tileVertexOffset(chunk, x, y);
    if ((*chunk.vertexArray)[start].color == color) {
        return;
    }
    ++drawRevision_;
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

    setTileColor(x, y, sf::Color::White);
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

std::optional<std::string> TileLayerGraphics::getMaterial(
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
    const int index = tileNumber.value();
    if (index < 0 || index >= getTilesetTileCount()) {
        return std::nullopt;
    }
    const auto found = materials_.find(index);
    return found == materials_.end() ? std::string{} : found->second;
}

int TileLayerGraphics::getTilesetTileCount() const {
    return texture_ && tileSize_ > 0
               ? static_cast<int>(texture_->getSize().x / tileSize_ *
                                  (texture_->getSize().y / tileSize_))
               : 0;
}

bool TileLayerGraphics::mayBlockLight(const sf::Vector2i& position) const {
    const auto path = getMaterial(position);
    return path.has_value() &&
           ludork::engine::materials::MaterialCompiler::analyse(
               *ludork::engine::materials::MaterialResource::load(*path))
               .mayBlockLight;
}

sf::Vector2f TileLayerGraphics::maximumMaterialOffset() const {
    return maximumMaterialOffset_;
}

bool TileLayerGraphics::hasReflectiveMaterial(
    const ludork::engine::materials::MaterialResource* layer,
    const sf::FloatRect& bounds) const {
    const sf::Vector2f offset =
        maximumMaterialOffset_ +
        (layer ? layer->maximumVertexOffset() : sf::Vector2f{});
    const sf::FloatRect expanded{bounds.position - offset,
                                 bounds.size + offset * 2.0f};
    const auto reflects = [layer, &expanded](const auto& runs) {
        return std::any_of(
            runs.begin(), runs.end(), [layer, &expanded](const auto& run) {
                return run.bounds.findIntersection(expanded).has_value() &&
                       ludork::engine::materials::MaterialCompiler::analyse(
                           *run.material, RuntimeData::Map{}, layer)
                           .mayReflect;
            });
    };
    for (const auto& chunk : chunks_) {
        if (!sf::FloatRect(
                 sf::Vector2f(chunk.x, chunk.y) * static_cast<float>(tileSize_),
                 sf::Vector2f(chunk.width, chunk.height) *
                     static_cast<float>(tileSize_))
                 .findIntersection(expanded)) {
            continue;
        }
        if (reflects(chunk.materialRuns)) {
            return true;
        }
        for (const auto& runs : chunk.autoTileMaterialRuns) {
            if (reflects(runs)) {
                return true;
            }
        }
    }
    return false;
}

bool TileLayerGraphics::hasAnimatedOcclusion(
    const ludork::engine::materials::MaterialResource* layer) const {
    const auto animated = [layer](const auto& runs) {
        return std::any_of(runs.begin(), runs.end(), [layer](const auto& run) {
            return ludork::engine::materials::MaterialCompiler::analyse(
                       *run.material, RuntimeData::Map{}, layer)
                .occlusionTimeDependent;
        });
    };
    for (const auto& chunk : chunks_) {
        if (animated(chunk.materialRuns)) {
            return true;
        }
        for (const auto& runs : chunk.autoTileMaterialRuns) {
            if (animated(runs)) {
                return true;
            }
        }
    }
    return false;
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
            ++drawRevision_;
            autoTileCurrentFrames_[i] = next;
            refreshAutoTileTexCoords(static_cast<int>(i));
        }
    }
}

void TileLayerGraphics::setAnimationTime(float seconds, float frameInterval) {
    if (!std::isfinite(seconds) || !std::isfinite(frameInterval) ||
        frameInterval <= 0.0f) {
        throw std::invalid_argument(
            "Tile animation time must be finite and interval positive");
    }
    const double steps =
        std::floor(static_cast<double>(seconds) / frameInterval);
    for (std::size_t index = 0; index < autoTileCurrentFrames_.size();
         ++index) {
        const int count = index < autoTileFrameCounts_.size()
                              ? autoTileFrameCounts_[index]
                              : 1;
        if (count <= 1) {
            continue;
        }
        const int frame =
            static_cast<int>(std::fmod(std::fmod(steps, count) + count, count));
        if (autoTileCurrentFrames_[index] != frame) {
            ++drawRevision_;
            autoTileCurrentFrames_[index] = frame;
            refreshAutoTileTexCoords(static_cast<int>(index));
        }
    }
}

void TileLayerGraphics::updateGraphicsBlock(int x, int y,
                                            const TileGrid& tileBlock,
                                            const AutoTileGrid& autoTileBlock) {
    if (tileBlock.empty() || tileBlock.front().empty()) {
        return;
    }
    ++drawRevision_;
    for (std::size_t row = 0; row < tileBlock.size(); ++row) {
        std::copy(tileBlock[row].begin(), tileBlock[row].end(),
                  tiles_[y + row].begin() + x);
        std::copy(autoTileBlock[row].begin(), autoTileBlock[row].end(),
                  autoTiles_[y + row].begin() + x);
    }
    const int firstX = std::max(0, x - 1) / ChunkSize;
    const int firstY = std::max(0, y - 1) / ChunkSize;
    const int lastX = std::min(static_cast<int>(size_.x) - 1,
                               x + static_cast<int>(tileBlock.front().size())) /
                      ChunkSize;
    const int lastY = std::min(static_cast<int>(size_.y) - 1,
                               y + static_cast<int>(tileBlock.size())) /
                      ChunkSize;
    for (int chunkY = firstY; chunkY <= lastY; ++chunkY) {
        for (int chunkX = firstX; chunkX <= lastX; ++chunkX) {
            const std::size_t index =
                ludork::engine::tilemap_graphics_impl::chunkIndex(
                    chunkX, chunkY, chunkColumns_);
            if (!builtChunks_[index]) {
                continue;
            }
            TileChunk& chunk = chunks_[index];
            chunk.vertexArray.reset();
            buildStaticChunk(chunk);
            buildAutoTileChunk(chunk);
        }
    }
}

void TileLayerGraphics::copyDisplayAutoTileMasks(
    const TileLayerGraphics& source,
    const std::vector<std::vector<sf::Vector2i>>& sources) {
    for (TileChunk& chunk : chunks_) {
        for (std::size_t pool = 0; pool < chunk.autoTileCells.size(); ++pool) {
            for (std::size_t cell = 0; cell < chunk.autoTileCells[pool].size();
                 ++cell) {
                const auto [x, y] = chunk.autoTileCells[pool][cell];
                const sf::Vector2i origin = sources[y][x];
                chunk.autoTileMasks[pool][cell] =
                    ludork::engine::tilemap_graphics_impl::autoTileMask(
                        source.autoTiles_, origin.x, origin.y,
                        static_cast<int>(pool));
            }
            refreshAutoTileTexCoords(chunk, static_cast<int>(pool));
        }
    }
}

void TileLayerGraphics::copyAutoTileAnimation(const TileLayerGraphics& source) {
    autoTileAnimationAccum_ = source.autoTileAnimationAccum_;
    for (std::size_t pool = 0; pool < autoTileCurrentFrames_.size(); ++pool) {
        if (autoTileCurrentFrames_[pool] !=
            source.autoTileCurrentFrames_[pool]) {
            ++drawRevision_;
            autoTileCurrentFrames_[pool] = source.autoTileCurrentFrames_[pool];
            refreshAutoTileTexCoords(static_cast<int>(pool));
        }
    }
}

int TileLayerGraphics::getLastVisibleChunkCount() const {
    return lastVisibleChunkCount_.load(std::memory_order_relaxed);
}

void TileLayerGraphics::draw(sf::RenderTarget& target,
                             sf::RenderStates states) const {
    ludork::engine::materials::MaterialProgramCache::Context context;
    drawMaterial(target, states, materialPrograms_, nullptr,
                 ludork::engine::materials::MaterialCompiler::Pass::Color,
                 context);
}

void TileLayerGraphics::drawMaterial(
    sf::RenderTarget& target, sf::RenderStates states,
    ludork::engine::materials::MaterialProgramCache& programs,
    const ludork::engine::materials::MaterialResource* layer,
    ludork::engine::materials::MaterialCompiler::Pass pass,
    ludork::engine::materials::MaterialProgramCache::Context context) const {
    lastVisibleChunkCount_.store(0, std::memory_order_relaxed);
    states.transform *= getTransform();
    if (chunkColumns_ == 0 || chunkRows_ == 0 || tileSize_ <= 0) {
        return;
    }
    auto visible = ludork::engine::tilemap_graphics_impl::localViewBounds(
        target, states.transform);
    const sf::Vector2f offset =
        maximumMaterialOffset_ +
        (layer ? layer->maximumVertexOffset() : sf::Vector2f{});
    visible.left -= offset.x;
    visible.right += offset.x;
    visible.top -= offset.y;
    visible.bottom += offset.y;
    const ChunkRange range =
        ludork::engine::tilemap_graphics_impl::visibleChunkRange(
            visible, size_, tileSize_, ChunkSize, chunkColumns_, chunkRows_);
    context.tile = true;
    context.cellSize = static_cast<float>(tileSize_);
    context.worldTransform = states.transform;
    context.targetSize = target.getSize();
    const auto drawRuns = [&](const sf::VertexArray& vertices,
                              const std::vector<TileChunk::MaterialRun>& runs) {
        for (const auto& run : runs) {
            context.textureSize = run.texture->getSize();
            const auto program =
                programs.bind(*run.material, {}, layer, {}, pass, context);
            sf::RenderStates drawStates = states;
            drawStates.shader = program.get();
            drawStates.texture = run.texture.get();
            target.draw(&vertices[run.first], run.count,
                        sf::PrimitiveType::Triangles, drawStates);
        }
    };
    int submitted = 0;
    for (int y = range.firstY; y <= range.lastY; ++y) {
        for (int x = range.firstX; x <= range.lastX; ++x) {
            const auto& chunk =
                chunks_[ludork::engine::tilemap_graphics_impl::chunkIndex(
                    x, y, chunkColumns_)];
            if (chunk.vertexArray) {
                drawRuns(*chunk.vertexArray, chunk.materialRuns);
            }
            if (!chunk.materialRuns.empty() ||
                std::any_of(chunk.autoTileMaterialRuns.begin(),
                            chunk.autoTileMaterialRuns.end(),
                            [](const auto& runs) {
                                return !runs.empty();
                            })) {
                ++submitted;
            }
        }
    }
    for (std::size_t pool = 0; pool < autoTileTextures_.size(); ++pool) {
        for (int y = range.firstY; y <= range.lastY; ++y) {
            for (int x = range.firstX; x <= range.lastX; ++x) {
                const auto& chunk =
                    chunks_[ludork::engine::tilemap_graphics_impl::chunkIndex(
                        x, y, chunkColumns_)];
                if (pool < chunk.autoTileVertexArrays.size() &&
                    chunk.autoTileVertexArrays[pool]) {
                    drawRuns(*chunk.autoTileVertexArrays[pool],
                             chunk.autoTileMaterialRuns[pool]);
                }
            }
        }
    }
    lastVisibleChunkCount_.store(submitted, std::memory_order_relaxed);
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
    const std::size_t index = ludork::engine::tilemap_graphics_impl::chunkIndex(
        chunkX, chunkY, chunkColumns_);
    if (builtChunks_[index]) {
        return buildComplete_;
    }
    TileChunk& chunk = chunks_[index];
    buildStaticChunk(chunk);
    buildAutoTileChunk(chunk);
    builtChunks_[index] = true;
    ++builtChunkCount_;
    ++drawRevision_;
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
    const std::size_t index = ludork::engine::tilemap_graphics_impl::chunkIndex(
        chunkX, chunkY, chunkColumns_);
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

std::size_t TileLayerGraphics::getDrawRevision() const {
    return drawRevision_;
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
                ludork::engine::tilemap_graphics_impl::chunkIndex(
                    chunkX, chunkY, chunkColumns_);
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
    chunk.materialRuns.clear();
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
            if (tileNumber < 0 || tileNumber >= getTilesetTileCount()) {
                continue;
            }
            if (chunk.vertexArray == nullptr) {
                chunk.vertexArray = std::make_unique<sf::VertexArray>(
                    sf::PrimitiveType::Triangles,
                    static_cast<std::size_t>(chunk.width * chunk.height * 6));
            }
            const int textureX = tileNumber % columns;
            const int textureY = tileNumber / columns;
            const int start =
                ludork::engine::tilemap_graphics_impl::tileVertexOffset(chunk,
                                                                        x, y);
            const auto positions =
                ludork::engine::tilemap_graphics_impl::rectangleVertices(
                    x * tileSize_, y * tileSize_, (x + 1) * tileSize_,
                    (y + 1) * tileSize_);
            const auto textureCoordinates =
                ludork::engine::tilemap_graphics_impl::rectangleVertices(
                    textureX * tileSize_, textureY * tileSize_,
                    (textureX + 1) * tileSize_, (textureY + 1) * tileSize_);
            const auto resource = materialResources_.find(tileNumber);
            const auto selected = resource == materialResources_.end()
                                      ? defaultMaterial_
                                      : resource->second;
            const sf::FloatRect cellBounds{{static_cast<float>(x * tileSize_),
                                            static_cast<float>(y * tileSize_)},
                                           sf::Vector2f(tileSize_, tileSize_)};
            if (!chunk.materialRuns.empty() &&
                chunk.materialRuns.back().material == selected &&
                chunk.materialRuns.back().first +
                        chunk.materialRuns.back().count ==
                    static_cast<std::size_t>(start)) {
                chunk.materialRuns.back().count += 6;
                chunk.materialRuns.back().bounds = combinedBounds(
                    chunk.materialRuns.back().bounds, cellBounds);
            } else {
                chunk.materialRuns.push_back({static_cast<std::size_t>(start),
                                              6, selected, texture_,
                                              cellBounds});
            }
            for (int vertex = 0; vertex < 6; ++vertex) {
                (*chunk.vertexArray)[start + vertex].position =
                    positions[vertex];
                (*chunk.vertexArray)[start + vertex].texCoords =
                    textureCoordinates[vertex];
                (*chunk.vertexArray)[start + vertex].color = sf::Color::White;
            }
        }
    }
}

void TileLayerGraphics::buildAutoTileChunk(TileChunk& chunk) {
    const std::size_t poolSize = autoTileTextures_.size();
    chunk.autoTileVertexArrays.clear();
    chunk.autoTileVertexArrays.resize(poolSize);
    chunk.autoTileMaterialRuns.assign(poolSize, {});
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
                const auto positions =
                    ludork::engine::tilemap_graphics_impl::rectangleVertices(
                        left, top, right, bottom);
                for (std::size_t vertex = 0; vertex < positions.size();
                     ++vertex) {
                    vertices[base + vertex].position = positions[vertex];
                }
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
    if (!autoTileAtlases_[poolIndex]) {
        return;
    }
    auto& runs = chunk.autoTileMaterialRuns[poolIndex];
    runs.clear();
    const std::vector<int>& masks = chunk.autoTileMasks[poolIndex];
    for (std::size_t cell = 0; cell < masks.size(); ++cell) {
        const auto& atlasFrame =
            autoTileAtlases_[poolIndex]->frame(masks[cell], frame);
        const auto& cellPosition = chunk.autoTileCells[poolIndex][cell];
        const sf::FloatRect cellBounds{
            sf::Vector2f(cellPosition.first, cellPosition.second) *
                static_cast<float>(tileSize_),
            sf::Vector2f(tileSize_, tileSize_)};
        if (!runs.empty() && runs.back().texture == atlasFrame.texture) {
            runs.back().count += 24;
            runs.back().bounds = combinedBounds(runs.back().bounds, cellBounds);
        } else {
            runs.push_back({cell * 24, 24, autoTileMaterials_[poolIndex],
                            atlasFrame.texture, cellBounds});
        }
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            const int sourceX =
                atlasFrame.rectangle.position.x + quadrant % 2 * half;
            const int sourceY =
                atlasFrame.rectangle.position.y + quadrant / 2 * half;
            const float left = static_cast<float>(sourceX);
            const float top = static_cast<float>(sourceY);
            const float right = left + static_cast<float>(half);
            const float bottom = top + static_cast<float>(half);
            const std::size_t base = (cell * 4 + quadrant) * 6;
            const auto coordinates =
                ludork::engine::tilemap_graphics_impl::rectangleVertices(
                    left, top, right, bottom);
            for (std::size_t vertex = 0; vertex < coordinates.size();
                 ++vertex) {
                (*vertices)[base + vertex].texCoords = coordinates[vertex];
            }
        }
    }
}
