#include <RenderTilemapRGBA.h>
#include <cstdint>
#include <pybind11/buffer_info.h>
#include <vector>

py::bytes C_RenderTilemapRGBA(py::buffer tilesetRgba, int tilesetW,
                              int tilesetH, int tilesetStride, py::buffer tiles,
                              int mapW, int mapH, int tileSize) {
  py::buffer_info tsInfo = tilesetRgba.request();
  if (tsInfo.ndim != 1) {
    throw std::runtime_error("tilesetRgba must be 1D buffer");
  }
  py::buffer_info tilesInfo = tiles.request();
  if (tilesInfo.ndim != 1) {
    throw std::runtime_error("tiles must be 1D buffer");
  }

  const std::uint8_t *ts = static_cast<std::uint8_t *>(tsInfo.ptr);
  const std::int32_t *t = static_cast<std::int32_t *>(tilesInfo.ptr);

  int outW = mapW * tileSize;
  int outH = mapH * tileSize;
  std::vector<std::uint8_t> out(outW * outH * 4, 0);

  int columns = tilesetW / tileSize;
  int rows = tilesetH / tileSize;
  int total = columns * rows;

  for (int y = 0; y < mapH; ++y) {
    for (int x = 0; x < mapW; ++x) {
      int idx = y * mapW + x;
      int n = t[idx];
      if (n < 0 || n >= total) {
        continue;
      }
      int tu = n % columns;
      int tv = n / columns;
      int srcX = tu * tileSize;
      int srcY = tv * tileSize;

      int dstX = x * tileSize;
      int dstY = y * tileSize;

      for (int py = 0; py < tileSize; ++py) {
        const std::uint8_t *srcRow =
            ts + (srcY + py) * tilesetStride + srcX * 4;
        std::uint8_t *dstRow = out.data() + (dstY + py) * outW * 4 + dstX * 4;
        std::memcpy(dstRow, srcRow, tileSize * 4);
      }
    }
  }

  return py::bytes(reinterpret_cast<const char *>(out.data()), out.size());
}
