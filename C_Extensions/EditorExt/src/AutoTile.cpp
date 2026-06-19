#include <AutoTile.h>
#include <AutoTileCore.h>

#include <pybind11/buffer_info.h>
#include <pybind11/stl.h>

#include <stdexcept>
#include <vector>

namespace {

RgbaImageView parseSourceTuple(const py::tuple &entry) {
    if (entry.size() < 4) {
        throw std::runtime_error("source tuple must be (rgba, w, h, stride)");
    }
    py::buffer_info info = entry[0].cast<py::buffer>().request();
    if (info.ndim != 1) {
        throw std::runtime_error("source rgba must be 1D buffer");
    }
    RgbaImageView src;
    src.data = static_cast<const std::uint8_t *>(info.ptr);
    src.w = entry[1].cast<int>();
    src.h = entry[2].cast<int>();
    src.stride = entry[3].cast<int>();
    if (src.w <= 0 || src.h <= 0 || src.stride < src.w * 4) {
        throw std::runtime_error("invalid source image dimensions");
    }
    if (info.size < static_cast<py::ssize_t>(src.h) * src.stride) {
        throw std::runtime_error("source rgba buffer too small");
    }
    return src;
}

}  // namespace

int C_NormalizeAutoTileMask(int mask) { return normalizeAutoTileMaskValue(mask); }

py::bytes C_ComposeAutoTileRGBA(py::buffer sourceRgba, int sourceW, int sourceH,
                                int sourceStride, int mask, int frame, int tileSize) {
    if (tileSize <= 0 || (tileSize % 2) != 0) {
        throw std::runtime_error("tileSize must be a positive even number");
    }
    RgbaImageView src = parseSourceTuple(py::make_tuple(sourceRgba, sourceW, sourceH, sourceStride));
    std::vector<std::uint8_t> out(tileSize * tileSize * 4, 0);
    composeAutoTileTileIntoBuffer(src, mask, frame, tileSize, out.data(), tileSize * 4);
    return py::bytes(reinterpret_cast<const char *>(out.data()), out.size());
}

int C_ComputeAutoTileMaskFromGrid(const std::vector<std::vector<std::string>> &grid, int x,
                                  int y) {
    return computeAutoTileMaskFromGrid(grid, x, y);
}

py::bytes C_RenderAutoTileLayerRGBA(int mapW, int mapH, int tileSize, int frame,
                                    const std::vector<std::vector<std::string>> &autoTileGrid,
                                    const py::dict &sourceRgbaByKey) {
    if (mapW <= 0 || mapH <= 0) {
        throw std::runtime_error("map dimensions must be positive");
    }
    if (tileSize <= 0 || (tileSize % 2) != 0) {
        throw std::runtime_error("tileSize must be a positive even number");
    }

    std::unordered_map<std::string, RgbaImageView> sources;
    sources.reserve(sourceRgbaByKey.size());
    for (auto item : sourceRgbaByKey) {
        std::string key = py::cast<std::string>(item.first);
        sources.emplace(key, parseSourceTuple(item.second.cast<py::tuple>()));
    }

    int outW = mapW * tileSize;
    int outH = mapH * tileSize;
    std::vector<std::uint8_t> out(outW * outH * 4, 0);
    renderAutoTileLayerOntoBuffer(mapW, mapH, tileSize, frame, autoTileGrid, sources, out.data(),
                                  outW, outH, outW * 4);
    return py::bytes(reinterpret_cast<const char *>(out.data()), out.size());
}
