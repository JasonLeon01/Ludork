#include <RenderTilemapRGBA.h>

#include <MapImageBuffer.h>

#include <pybind11/pybind11.h>
#include <pybind11/buffer_info.h>
#include <stdexcept>
#include <vector>

void renderTilemapOntoBuffer(const RgbaImageView &tileset, const std::int32_t *tiles, int mapW,
                             int mapH, int sourceTileSize, std::uint8_t *dst, int dstW, int dstH,
                             int dstStride) {
    if (sourceTileSize <= 0 || mapW <= 0 || mapH <= 0) {
        return;
    }
    int columns = tileset.w / sourceTileSize;
    int rows = tileset.h / sourceTileSize;
    if (columns <= 0 || rows <= 0) {
        return;
    }
    int total = columns * rows;

    for (int y = 0; y < mapH; ++y) {
        for (int x = 0; x < mapW; ++x) {
            int idx = y * mapW + x;
            int n = tiles[idx];
            if (n < 0 || n >= total) {
                continue;
            }
            int tu = n % columns;
            int tv = n / columns;
            int srcX = tu * sourceTileSize;
            int srcY = tv * sourceTileSize;
            int dstX = x * sourceTileSize;
            int dstY = y * sourceTileSize;
            blitRectCopy(tileset, srcX, srcY, sourceTileSize, sourceTileSize, dst, dstW, dstH,
                         dstStride, dstX, dstY);
        }
    }
}

py::bytes C_RenderTilemapRGBA(py::buffer tilesetRgba, int tilesetW, int tilesetH,
                              int tilesetStride, py::buffer tiles, int mapW, int mapH,
                              int sourceTileSize, int outputTileSize) {
    if (sourceTileSize <= 0 || outputTileSize <= 0) {
        throw std::runtime_error("tile sizes must be positive");
    }
    py::buffer_info tsInfo = tilesetRgba.request();
    if (tsInfo.ndim != 1) {
        throw std::runtime_error("tilesetRgba must be 1D buffer");
    }
    py::buffer_info tilesInfo = tiles.request();
    if (tilesInfo.ndim != 1) {
        throw std::runtime_error("tiles must be 1D buffer");
    }

    RgbaImageView tileset;
    tileset.data = static_cast<const std::uint8_t *>(tsInfo.ptr);
    tileset.w = tilesetW;
    tileset.h = tilesetH;
    tileset.stride = tilesetStride;

    const std::int32_t *tileIndices = static_cast<const std::int32_t *>(tilesInfo.ptr);
    int renderW = mapW * sourceTileSize;
    int renderH = mapH * sourceTileSize;
    std::vector<std::uint8_t> rendered(renderW * renderH * 4, 0);
    renderTilemapOntoBuffer(tileset, tileIndices, mapW, mapH, sourceTileSize, rendered.data(),
                            renderW, renderH, renderW * 4);

    if (outputTileSize == sourceTileSize) {
        return py::bytes(reinterpret_cast<const char *>(rendered.data()), rendered.size());
    }

    int outW = mapW * outputTileSize;
    int outH = mapH * outputTileSize;
    std::vector<std::uint8_t> out(outW * outH * 4, 0);
    scaleRgbaImage(rendered.data(), renderW, renderH, renderW * 4, out.data(), outW, outH,
                   outW * 4);
    return py::bytes(reinterpret_cast<const char *>(out.data()), out.size());
}
