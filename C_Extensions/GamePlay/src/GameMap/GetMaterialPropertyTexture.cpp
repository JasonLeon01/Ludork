#include <GameMap/GetMaterialPropertyTexture.h>
#include <pybind11/stl.h>

void C_GetMaterialPropertyTexture(
    const sf::Vector2u &size, sf::Image &img,
    const std::vector<std::vector<py::object>> &materialMap) {
  for (unsigned int y = 0; y < size.y; y++) {
    for (unsigned int x = 0; x < size.x; x++) {
      std::uint8_t g =
          static_cast<std::uint8_t>(materialMap[y][x].cast<float>() * 255);
      img.setPixel({x, y}, {g, g, g});
    }
  }
}

void ApplyGetMaterialPropertyTextureBinding(py::module &m) {
  m.def("C_GetMaterialPropertyTexture", &C_GetMaterialPropertyTexture,
        py::arg("size"), py::arg("img"), py::arg("materialMap"));
}
