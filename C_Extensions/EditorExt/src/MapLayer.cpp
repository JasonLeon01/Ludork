#include <MapLayer.h>

#include <AutoTileCore.h>
#include <MapImageBuffer.h>
#include <RenderTilemapRGBA.h>

#include <pybind11/buffer_info.h>
#include <pybind11/stl.h>

#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

RgbaImageView parseRgbaTuple(const py::tuple &entry) {
    if (entry.size() < 4) {
        throw std::runtime_error("rgba tuple must be (rgba, w, h, stride)");
    }
    py::buffer_info info = entry[0].cast<py::buffer>().request();
    if (info.ndim != 1) {
        throw std::runtime_error("rgba buffer must be 1D");
    }
    RgbaImageView view;
    view.data = static_cast<const std::uint8_t *>(info.ptr);
    view.w = entry[1].cast<int>();
    view.h = entry[2].cast<int>();
    view.stride = entry[3].cast<int>();
    if (view.w <= 0 || view.h <= 0 || view.stride < view.w * 4) {
        throw std::runtime_error("invalid rgba image dimensions");
    }
    if (info.size < static_cast<py::ssize_t>(view.h) * view.stride) {
        throw std::runtime_error("rgba buffer too small");
    }
    return view;
}

std::unordered_map<std::string, RgbaImageView> parseSourceDict(const py::dict &sources) {
    std::unordered_map<std::string, RgbaImageView> out;
    out.reserve(sources.size());
    for (auto item : sources) {
        std::string key = py::cast<std::string>(item.first);
        out.emplace(key, parseRgbaTuple(item.second.cast<py::tuple>()));
    }
    return out;
}

bool hasTileData(py::object tilesObj) {
  return !tilesObj.is_none();
}

bool hasAutoTileData(py::object gridObj) {
    if (gridObj.is_none()) {
        return false;
    }
    auto grid = gridObj.cast<std::vector<std::vector<std::string>>>();
    for (const auto &row : grid) {
        for (const auto &key : row) {
            if (!key.empty()) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

py::bytes C_RenderMapLayerRGBA(int mapW, int mapH, int sourceTileSize, int outputTileSize,
                               py::object tilesetRgbaTuple, py::object tiles, int autoTileFrame,
                               py::object autoTileGrid, py::object autoTileSourcesByKey) {
    if (mapW <= 0 || mapH <= 0) {
        throw std::runtime_error("map dimensions must be positive");
    }
    if (sourceTileSize <= 0 || outputTileSize <= 0) {
        throw std::runtime_error("tile sizes must be positive");
    }

    int renderW = mapW * sourceTileSize;
    int renderH = mapH * sourceTileSize;
    std::vector<std::uint8_t> rendered(renderW * renderH * 4, 0);

    if (!tilesetRgbaTuple.is_none() && hasTileData(tiles)) {
        RgbaImageView tileset = parseRgbaTuple(tilesetRgbaTuple.cast<py::tuple>());
        py::buffer_info tilesInfo = tiles.cast<py::buffer>().request();
        if (tilesInfo.ndim != 1) {
            throw std::runtime_error("tiles must be 1D buffer");
        }
        const std::int32_t *tileIndices = static_cast<const std::int32_t *>(tilesInfo.ptr);
        renderTilemapOntoBuffer(tileset, tileIndices, mapW, mapH, sourceTileSize, rendered.data(),
                                  renderW, renderH, renderW * 4);
    }

    if (hasAutoTileData(autoTileGrid)) {
        if (autoTileSourcesByKey.is_none()) {
            throw std::runtime_error("autotile sources are required when autoTileGrid is set");
        }
        auto grid = autoTileGrid.cast<std::vector<std::vector<std::string>>>();
        auto sources = parseSourceDict(autoTileSourcesByKey.cast<py::dict>());
        renderAutoTileLayerOntoBuffer(mapW, mapH, sourceTileSize, autoTileFrame, grid, sources,
                                      rendered.data(), renderW, renderH, renderW * 4);
    }

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
