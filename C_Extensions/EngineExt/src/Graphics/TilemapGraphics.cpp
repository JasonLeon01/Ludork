#include <Graphics/TilemapGraphics.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>

#include <array>

////////////////////////////////////////////////////////////
// Autotile mask helpers (RPG Maker XP 48-state composition).
//
// Bit layout of the 8-bit neighbour mask matches the Python-side
// `AutoTileRenderer`:
//   bit0=top, bit1=right, bit2=bottom, bit3=left,
//   bit4=top-left, bit5=top-right, bit6=bottom-right, bit7=bottom-left.
////////////////////////////////////////////////////////////
namespace {

constexpr int MASK_TOP = 0x01;
constexpr int MASK_RIGHT = 0x02;
constexpr int MASK_BOTTOM = 0x04;
constexpr int MASK_LEFT = 0x08;
constexpr int MASK_TOP_LEFT = 0x10;
constexpr int MASK_TOP_RIGHT = 0x20;
constexpr int MASK_BOTTOM_RIGHT = 0x40;
constexpr int MASK_BOTTOM_LEFT = 0x80;

constexpr int kInnerFillerCell = 3;

// Base 4-quadrant cell pattern (1-based mini-pattern indices) keyed by the
// 4-bit orthogonal mask. Order: (TL, TR, BL, BR).
constexpr std::array<std::array<int, 4>, 16> kBasePattern = {{
    {{1, 1, 1, 1}},      // 0x00
    {{10, 12, 10, 12}},  // 0x01
    {{4, 4, 10, 10}},    // 0x02
    {{10, 10, 10, 10}},  // 0x03
    {{4, 6, 4, 6}},      // 0x04
    {{7, 9, 7, 9}},      // 0x05
    {{4, 4, 4, 4}},      // 0x06
    {{7, 7, 7, 7}},      // 0x07
    {{6, 6, 12, 12}},    // 0x08
    {{12, 12, 12, 12}},  // 0x09
    {{5, 5, 11, 11}},    // 0x0A
    {{11, 11, 11, 11}},  // 0x0B
    {{6, 6, 6, 6}},      // 0x0C
    {{9, 9, 9, 9}},      // 0x0D
    {{5, 5, 5, 5}},      // 0x0E
    {{8, 8, 8, 8}}       // 0x0F
}};

// (orthoA, orthoB, diagonal) for each quadrant (TL, TR, BL, BR).
constexpr std::array<std::array<int, 3>, 4> kQuadBits = {{
    {{MASK_TOP, MASK_LEFT, MASK_TOP_LEFT}},
    {{MASK_TOP, MASK_RIGHT, MASK_TOP_RIGHT}},
    {{MASK_BOTTOM, MASK_LEFT, MASK_BOTTOM_LEFT}},
    {{MASK_BOTTOM, MASK_RIGHT, MASK_BOTTOM_RIGHT}},
}};

std::array<int, 4> composeCellPattern(int mask) {
    int orthoMask = mask & 0x0F;
    std::array<int, 4> out = kBasePattern[orthoMask];
    for (int q = 0; q < 4; ++q) {
        int oa = kQuadBits[q][0];
        int ob = kQuadBits[q][1];
        int d = kQuadBits[q][2];
        if ((mask & oa) && (mask & ob) && !(mask & d)) {
            out[q] = kInnerFillerCell;
        }
    }
    return out;
}

std::optional<int> autoTileIndexAt(const AutoTileGrid &grid, int x, int y) {
    if (y < 0 || y >= static_cast<int>(grid.size())) {
        return std::nullopt;
    }
    const auto &row = grid[y];
    if (x < 0 || x >= static_cast<int>(row.size())) {
        return std::nullopt;
    }
    const auto &cell = row[x];
    if (!cell.has_value()) {
        return std::nullopt;
    }
    if (const auto index = std::get_if<int>(&cell.value())) {
        return *index;
    }
    return std::nullopt;
}

bool sameAutoTileAt(const AutoTileGrid &grid, int x, int y, int width, int height, int self) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    auto cellIndex = autoTileIndexAt(grid, x, y);
    if (!cellIndex.has_value()) {
        return false;
    }
    return cellIndex.value() == self;
}

int computeAutoTileMask(const AutoTileGrid &grid, int x, int y, int width, int height) {
    auto selfIndex = autoTileIndexAt(grid, x, y);
    if (!selfIndex.has_value()) {
        return 0;
    }
    int self = selfIndex.value();
    int mask = 0;
    if (sameAutoTileAt(grid, x, y - 1, width, height, self)) mask |= MASK_TOP;
    if (sameAutoTileAt(grid, x + 1, y, width, height, self)) mask |= MASK_RIGHT;
    if (sameAutoTileAt(grid, x, y + 1, width, height, self)) mask |= MASK_BOTTOM;
    if (sameAutoTileAt(grid, x - 1, y, width, height, self)) mask |= MASK_LEFT;
    if (sameAutoTileAt(grid, x - 1, y - 1, width, height, self)) mask |= MASK_TOP_LEFT;
    if (sameAutoTileAt(grid, x + 1, y - 1, width, height, self)) mask |= MASK_TOP_RIGHT;
    if (sameAutoTileAt(grid, x + 1, y + 1, width, height, self)) mask |= MASK_BOTTOM_RIGHT;
    if (sameAutoTileAt(grid, x - 1, y + 1, width, height, self)) mask |= MASK_BOTTOM_LEFT;
    return mask;
}

}  // namespace

TileLayerGraphics::TileLayerGraphics(int width, int height, int tileSize, sf::Texture *texture,
                                     const TileLayerData &data,
                                     const std::vector<sf::Texture *> &autoTileTextures,
                                     const std::vector<int> &autoTileFrameCounts) {
    texture_ = texture;
    vertexArray_ = new sf::VertexArray(sf::PrimitiveType::Triangles, width * height * 6);
    size_ = sf::Vector2f(width, height);
    tileSize_ = tileSize;
    data_ = data;
    tiles_ = data.tiles;
    materials_ = data.layerTileset.materials;
    autoTiles_ = data.autoTiles;
    autoTilePool_ = data.autoTilePool;
    autoTileTextures_ = autoTileTextures;
    autoTileMaterials_.reserve(autoTilePool_.size());
    for (const auto &autoTile : autoTilePool_) {
        autoTileMaterials_.push_back(autoTile.material);
    }
    autoTileFrameCounts_ = autoTileFrameCounts;
    autoTileCurrentFrames_.assign(autoTileTextures_.size(), 0);
    autoTileAnimationAccum_ = 0.0f;
    init(tileSize);
    initAutoTiles(tileSize);
}

TileLayerGraphics::~TileLayerGraphics() {
    delete vertexArray_;
    for (auto *va : autoTileVertexArrays_) {
        delete va;
    }
}

void TileLayerGraphics::setTileColor(int x, int y, sf::Color color) {
    int width = static_cast<int>(size_.x);
    if (x < 0 || x >= width || y < 0 || y >= static_cast<int>(size_.y)) return;

    if (!tiles_[y][x].has_value()) return;

    int start = (x + y * width) * 6;
    for (int i = 0; i < 6; ++i) {
        (*vertexArray_)[start + i].color = color;
    }
}

void TileLayerGraphics::resetTileColor(int x, int y) {
    int width = static_cast<int>(size_.x);
    if (x < 0 || x >= width || y < 0 || y >= static_cast<int>(size_.y)) return;

    if (!tiles_[y][x].has_value()) return;

    int tileNumber = tiles_[y][x].value();
    if (tileNumber < 0 || tileNumber >= static_cast<int>(materials_.size())) return;

    float opacity = materials_[tileNumber].opacity;
    sf::Color color = sf::Color::White;
    color.a = static_cast<std::uint8_t>(opacity * 255);

    setTileColor(x, y, color);
}

std::vector<std::pair<int, int>> TileLayerGraphics::floodFillTransparent(int startX, int startY, sf::Color color) {
    std::vector<std::pair<int, int>> processedTiles;
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);

    if (startX < 0 || startX >= width || startY < 0 || startY >= height) return processedTiles;

    if (!tiles_[startY][startX].has_value()) return processedTiles;

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

bool TileLayerGraphics::inBounds(const sf::Vector2i &position) const {
    return position.x >= 0 && position.y >= 0 && position.x < static_cast<int>(size_.x) &&
           position.y < static_cast<int>(size_.y);
}

std::optional<int> TileLayerGraphics::getAutoTileIndex(const sf::Vector2i &position) const {
    if (!inBounds(position)) {
        return std::nullopt;
    }
    auto index = autoTileIndexAt(autoTiles_, position.x, position.y);
    if (!index.has_value()) {
        return std::nullopt;
    }
    if (index.value() < 0 || index.value() >= static_cast<int>(autoTilePool_.size())) {
        return std::nullopt;
    }
    return index;
}

std::optional<int> TileLayerGraphics::get(const sf::Vector2i &position) const {
    if (!inBounds(position)) {
        return std::nullopt;
    }
    if (position.y >= static_cast<int>(tiles_.size())) {
        return std::nullopt;
    }
    const auto &row = tiles_[position.y];
    if (position.x >= static_cast<int>(row.size())) {
        return std::nullopt;
    }
    return row[position.x];
}

std::optional<AutoTile> TileLayerGraphics::getAutoTileAt(const sf::Vector2i &position) const {
    auto index = getAutoTileIndex(position);
    if (!index.has_value()) {
        return std::nullopt;
    }
    return autoTilePool_[index.value()];
}

bool TileLayerGraphics::isPassable(const sf::Vector2i &position) const {
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
    if (index < 0 || index >= static_cast<int>(data_.layerTileset.passable.size())) {
        return false;
    }
    return data_.layerTileset.passable[index];
}

std::optional<Material> TileLayerGraphics::getMaterial(const sf::Vector2i &position) const {
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
    std::vector<std::vector<float>> result(height, std::vector<float>(width, 0.0f));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto material = getMaterial(sf::Vector2i(x, y));
            result[y][x] = material.has_value() ? material.value().lightBlock : 0.0f;
        }
    }
    return result;
}

std::vector<std::vector<float>> TileLayerGraphics::getReflectionStrengthMap() const {
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);
    std::vector<std::vector<float>> result(height, std::vector<float>(width, 0.0f));
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

std::vector<std::vector<float>> TileLayerGraphics::getIgnoreLightingMap() const {
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);
    std::vector<std::vector<float>> result(height, std::vector<float>(width, 0.0f));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto material = getMaterial(sf::Vector2i(x, y));
            result[y][x] = material.has_value() && material.value().ignoreLighting ? 1.0f : 0.0f;
        }
    }
    return result;
}

void TileLayerGraphics::updateAutoTileAnimation(float deltaTime, float frameInterval) {
    if (autoTileVertexArrays_.empty() || frameInterval <= 0.0f) {
        return;
    }
    autoTileAnimationAccum_ += deltaTime;
    if (autoTileAnimationAccum_ < frameInterval) {
        return;
    }
    int steps = static_cast<int>(autoTileAnimationAccum_ / frameInterval);
    autoTileAnimationAccum_ -= steps * frameInterval;
    for (std::size_t i = 0; i < autoTileVertexArrays_.size(); ++i) {
        int frameCount = i < autoTileFrameCounts_.size() ? autoTileFrameCounts_[i] : 1;
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

void TileLayerGraphics::draw(sf::RenderTarget &target, sf::RenderStates states) const {
    states.transform *= getTransform();
    sf::RenderStates tileStates = states;
    tileStates.texture = texture_;
    target.draw(*vertexArray_, tileStates);
    for (std::size_t i = 0; i < autoTileVertexArrays_.size(); ++i) {
        if (autoTileVertexArrays_[i] == nullptr) continue;
        if (autoTileVertexArrays_[i]->getVertexCount() == 0) continue;
        sf::RenderStates autoStates = states;
        autoStates.texture = autoTileTextures_[i];
        target.draw(*autoTileVertexArrays_[i], autoStates);
    }
}

void TileLayerGraphics::init(int tileSize) {
    if (texture_ == nullptr) {
        return;
    }
    int columns = texture_->getSize().x / tileSize;
    int width = size_.x;
    int height = size_.y;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto tileNumberObj = tiles_[y][x];
            if (!tileNumberObj.has_value()) {
                continue;
            }
            int tileNumber = tileNumberObj.value();
            if (tileNumber < 0 || tileNumber >= static_cast<int>(materials_.size())) {
                continue;
            }

            int tu = tileNumber % columns;
            int tv = tileNumber / columns;
            int start = (x + y * width) * 6;

            float opacity = materials_[tileNumber].opacity;
            sf::Color color = sf::Color::White;
            color.a = static_cast<std::uint8_t>(opacity * 255);

            std::vector<sf::Vector2f> positions = {
                sf::Vector2f(x * tileSize, y * tileSize),       sf::Vector2f((x + 1) * tileSize, y * tileSize),
                sf::Vector2f(x * tileSize, (y + 1) * tileSize), sf::Vector2f(x * tileSize, (y + 1) * tileSize),
                sf::Vector2f((x + 1) * tileSize, y * tileSize), sf::Vector2f((x + 1) * tileSize, (y + 1) * tileSize),
            };
            std::vector<sf::Vector2f> texCoords = {
                sf::Vector2f(tu * tileSize, tv * tileSize),
                sf::Vector2f((tu + 1) * tileSize, tv * tileSize),
                sf::Vector2f(tu * tileSize, (tv + 1) * tileSize),
                sf::Vector2f(tu * tileSize, (tv + 1) * tileSize),
                sf::Vector2f((tu + 1) * tileSize, tv * tileSize),
                sf::Vector2f((tu + 1) * tileSize, (tv + 1) * tileSize),
            };

            for (int i = 0; i < 6; ++i) {
                if (opacity > 0.0f) {
                    (*vertexArray_)[start + i].position = positions[i];
                    (*vertexArray_)[start + i].texCoords = texCoords[i];
                    if (opacity < 1.0f) {
                        (*vertexArray_)[start + i].color = color;
                    }
                }
            }
        }
    }
}

void TileLayerGraphics::initAutoTiles(int tileSize) {
    int width = static_cast<int>(size_.x);
    int height = static_cast<int>(size_.y);
    std::size_t poolSize = autoTileTextures_.size();
    autoTileVertexArrays_.assign(poolSize, nullptr);
    autoTileCells_.assign(poolSize, {});
    autoTileMasks_.assign(poolSize, {});

    if (poolSize == 0 || autoTiles_.empty()) {
        return;
    }

    for (std::size_t i = 0; i < poolSize; ++i) {
        autoTileVertexArrays_[i] = new sf::VertexArray(sf::PrimitiveType::Triangles, 0);
    }

    for (int y = 0; y < height; ++y) {
        if (y >= static_cast<int>(autoTiles_.size())) break;
        auto &row = autoTiles_[y];
        for (int x = 0; x < width; ++x) {
            if (x >= static_cast<int>(row.size())) break;
            auto poolIndexValue = autoTileIndexAt(autoTiles_, x, y);
            if (!poolIndexValue.has_value()) continue;
            int poolIndex = poolIndexValue.value();
            if (poolIndex < 0 || poolIndex >= static_cast<int>(poolSize)) continue;
            int mask = computeAutoTileMask(autoTiles_, x, y, width, height);
            autoTileCells_[poolIndex].push_back({x, y});
            autoTileMasks_[poolIndex].push_back(mask);
        }
    }

    for (std::size_t i = 0; i < poolSize; ++i) {
        std::size_t cellCount = autoTileCells_[i].size();
        autoTileVertexArrays_[i]->resize(cellCount * 4 * 6);

        for (std::size_t k = 0; k < cellCount; ++k) {
            int cx = autoTileCells_[i][k].first;
            int cy = autoTileCells_[i][k].second;
            int half = tileSize / 2;
            for (int q = 0; q < 4; ++q) {
                int qx = q % 2;
                int qy = q / 2;
                float px0 = static_cast<float>(cx * tileSize + qx * half);
                float py0 = static_cast<float>(cy * tileSize + qy * half);
                float px1 = px0 + half;
                float py1 = py0 + half;
                std::size_t base = (k * 4 + q) * 6;
                (*autoTileVertexArrays_[i])[base + 0].position = sf::Vector2f(px0, py0);
                (*autoTileVertexArrays_[i])[base + 1].position = sf::Vector2f(px1, py0);
                (*autoTileVertexArrays_[i])[base + 2].position = sf::Vector2f(px0, py1);
                (*autoTileVertexArrays_[i])[base + 3].position = sf::Vector2f(px0, py1);
                (*autoTileVertexArrays_[i])[base + 4].position = sf::Vector2f(px1, py0);
                (*autoTileVertexArrays_[i])[base + 5].position = sf::Vector2f(px1, py1);
            }
        }

        float opacity = i < autoTileMaterials_.size() ? autoTileMaterials_[i].opacity : 1.0f;
        sf::Color color = sf::Color::White;
        color.a = static_cast<std::uint8_t>(opacity * 255);
        if (opacity < 1.0f) {
            for (std::size_t v = 0; v < autoTileVertexArrays_[i]->getVertexCount(); ++v) {
                (*autoTileVertexArrays_[i])[v].color = color;
            }
        }

        refreshAutoTileTexCoords(static_cast<int>(i));
    }
}

void TileLayerGraphics::refreshAutoTileTexCoords(int poolIndex) {
    if (poolIndex < 0 || poolIndex >= static_cast<int>(autoTileVertexArrays_.size())) return;
    sf::VertexArray *va = autoTileVertexArrays_[poolIndex];
    if (va == nullptr) return;
    int half = tileSize_ / 2;
    int frameCount = poolIndex < static_cast<int>(autoTileFrameCounts_.size()) ? autoTileFrameCounts_[poolIndex] : 1;
    if (frameCount <= 0) frameCount = 1;
    int frame = autoTileCurrentFrames_[poolIndex] % frameCount;
    int frameOffsetX = frame * 3 * tileSize_;

    auto &cells = autoTileCells_[poolIndex];
    auto &masks = autoTileMasks_[poolIndex];
    for (std::size_t k = 0; k < cells.size(); ++k) {
        std::array<int, 4> pattern = composeCellPattern(masks[k]);
        for (int q = 0; q < 4; ++q) {
            int cell0Based = pattern[q] - 1;
            int col = cell0Based % 3;
            int row = cell0Based / 3;
            int cellX = col * tileSize_;
            int cellY = row * tileSize_;
            int qx = q % 2;
            int qy = q / 2;
            int srcX = cellX + qx * half + frameOffsetX;
            int srcY = cellY + qy * half;
            float tx0 = static_cast<float>(srcX);
            float ty0 = static_cast<float>(srcY);
            float tx1 = tx0 + half;
            float ty1 = ty0 + half;
            std::size_t base = (k * 4 + q) * 6;
            (*va)[base + 0].texCoords = sf::Vector2f(tx0, ty0);
            (*va)[base + 1].texCoords = sf::Vector2f(tx1, ty0);
            (*va)[base + 2].texCoords = sf::Vector2f(tx0, ty1);
            (*va)[base + 3].texCoords = sf::Vector2f(tx0, ty1);
            (*va)[base + 4].texCoords = sf::Vector2f(tx1, ty0);
            (*va)[base + 5].texCoords = sf::Vector2f(tx1, ty1);
        }
    }
}
