#include <AutoTileCore.h>

#include <MapImageBuffer.h>

#include <array>
#include <algorithm>
#include <unordered_map>
#include <vector>

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

int frameCountForSource(int sourceW, int tileSize) {
    if (sourceW <= 0 || tileSize <= 0) {
        return 1;
    }
    return std::max(1, sourceW / (3 * tileSize));
}

bool sameKeyAt(const std::vector<std::vector<std::string>> &grid, int x, int y, int width,
               int height, const std::string &key) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    if (y >= static_cast<int>(grid.size())) {
        return false;
    }
    const auto &row = grid[y];
    if (x >= static_cast<int>(row.size())) {
        return false;
    }
    return row[x] == key;
}

}  // namespace

int normalizeAutoTileMaskValue(int mask) {
    int result = mask & 0x0F;
    if ((mask & MASK_TOP) && (mask & MASK_LEFT) && (mask & MASK_TOP_LEFT)) {
        result |= MASK_TOP_LEFT;
    }
    if ((mask & MASK_TOP) && (mask & MASK_RIGHT) && (mask & MASK_TOP_RIGHT)) {
        result |= MASK_TOP_RIGHT;
    }
    if ((mask & MASK_BOTTOM) && (mask & MASK_RIGHT) && (mask & MASK_BOTTOM_RIGHT)) {
        result |= MASK_BOTTOM_RIGHT;
    }
    if ((mask & MASK_BOTTOM) && (mask & MASK_LEFT) && (mask & MASK_BOTTOM_LEFT)) {
        result |= MASK_BOTTOM_LEFT;
    }
    return result;
}

int computeAutoTileMaskFromGrid(const std::vector<std::vector<std::string>> &grid, int x, int y) {
    if (y < 0 || y >= static_cast<int>(grid.size())) {
        return 0;
    }
    const auto &row = grid[y];
    if (x < 0 || x >= static_cast<int>(row.size())) {
        return 0;
    }
    const std::string &key = row[x];
    if (key.empty()) {
        return 0;
    }
    int height = static_cast<int>(grid.size());
    int width = static_cast<int>(row.size());
    int mask = 0;
    if (sameKeyAt(grid, x, y - 1, width, height, key)) mask |= MASK_TOP;
    if (sameKeyAt(grid, x + 1, y, width, height, key)) mask |= MASK_RIGHT;
    if (sameKeyAt(grid, x, y + 1, width, height, key)) mask |= MASK_BOTTOM;
    if (sameKeyAt(grid, x - 1, y, width, height, key)) mask |= MASK_LEFT;
    if (sameKeyAt(grid, x - 1, y - 1, width, height, key)) mask |= MASK_TOP_LEFT;
    if (sameKeyAt(grid, x + 1, y - 1, width, height, key)) mask |= MASK_TOP_RIGHT;
    if (sameKeyAt(grid, x + 1, y + 1, width, height, key)) mask |= MASK_BOTTOM_RIGHT;
    if (sameKeyAt(grid, x - 1, y + 1, width, height, key)) mask |= MASK_BOTTOM_LEFT;
    return mask;
}

void composeAutoTileTileIntoBuffer(const RgbaImageView &src, int mask, int frame, int tileSize,
                                   std::uint8_t *out, int outStride) {
    int half = tileSize / 2;
    int frames = frameCountForSource(src.w, tileSize);
    int frameMod = frames > 0 ? ((frame % frames) + frames) % frames : 0;
    int frameOffsetX = frameMod * 3 * tileSize;
    int normalized = normalizeAutoTileMaskValue(mask);
    std::array<int, 4> cells = composeCellPattern(normalized);

    for (int quadrant = 0; quadrant < 4; ++quadrant) {
        int qx = quadrant % 2;
        int qy = quadrant / 2;
        int cell0Based = cells[quadrant] - 1;
        int col = cell0Based % 3;
        int row = cell0Based / 3;
        int cellX = col * tileSize;
        int cellY = row * tileSize;
        int srcX = cellX + qx * half + frameOffsetX;
        int srcY = cellY + qy * half;
        blitRectCopy(src, srcX, srcY, half, half, out, tileSize, tileSize, outStride, qx * half,
                     qy * half);
    }
}

void renderAutoTileLayerOntoBuffer(
    int mapW, int mapH, int sourceTileSize, int frame,
    const std::vector<std::vector<std::string>> &autoTileGrid,
    const std::unordered_map<std::string, RgbaImageView> &sources, std::uint8_t *dst, int dstW,
    int dstH, int dstStride) {
    if (sourceTileSize <= 0 || mapW <= 0 || mapH <= 0) {
        return;
    }

    std::unordered_map<std::string, std::vector<std::uint8_t>> tileCache;
    int gridHeight = static_cast<int>(autoTileGrid.size());

    for (int y = 0; y < gridHeight; ++y) {
        const auto &row = autoTileGrid[y];
        int rowWidth = static_cast<int>(row.size());
        for (int x = 0; x < rowWidth; ++x) {
            const std::string &key = row[x];
            if (key.empty()) {
                continue;
            }
            auto srcIt = sources.find(key);
            if (srcIt == sources.end()) {
                continue;
            }
            const RgbaImageView &src = srcIt->second;
            int mask = computeAutoTileMaskFromGrid(autoTileGrid, x, y);
            int normalized = normalizeAutoTileMaskValue(mask);
            int frames = frameCountForSource(src.w, sourceTileSize);
            int frameMod = frames > 0 ? ((frame % frames) + frames) % frames : 0;

            std::string cacheKey = key + "\x1f" + std::to_string(normalized) + "\x1f" +
                                   std::to_string(frameMod);
            auto cacheIt = tileCache.find(cacheKey);
            const std::uint8_t *tileBytes = nullptr;
            if (cacheIt != tileCache.end()) {
                tileBytes = cacheIt->second.data();
            } else {
                std::vector<std::uint8_t> tile(sourceTileSize * sourceTileSize * 4, 0);
                composeAutoTileTileIntoBuffer(src, mask, frame, sourceTileSize, tile.data(),
                                                sourceTileSize * 4);
                auto inserted = tileCache.emplace(std::move(cacheKey), std::move(tile));
                tileBytes = inserted.first->second.data();
            }

            int dstX = x * sourceTileSize;
            int dstY = y * sourceTileSize;
            RgbaImageView tileView{tileBytes, sourceTileSize, sourceTileSize, sourceTileSize * 4};
            blitRectCopy(tileView, 0, 0, sourceTileSize, sourceTileSize, dst, dstW, dstH, dstStride,
                         dstX, dstY);
        }
    }
}
