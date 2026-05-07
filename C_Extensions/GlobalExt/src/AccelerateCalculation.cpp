#include <pybind11/stl.h>

#include <AccelerateCalculation.hpp>


void C_ImageUpdateBuffer1D(sf::Texture &img, py::buffer buffer) {
    py::buffer_info info = buffer.request();
    if (info.format != py::format_descriptor<uint8_t>::format()) {
        throw std::runtime_error("Expected a uint8 buffer");
    }
    if (info.ndim != 1) {
        throw std::runtime_error("Expected a 1D buffer");
    }
    std::uint8_t *ptr = static_cast<std::uint8_t *>(info.ptr);
    img.update(ptr);
}

void C_ImageUpdateBuffer3D(sf::Texture &img, py::buffer buffer) {
    py::buffer_info info = buffer.request();
    if (info.ndim != 3 || info.shape[2] != 4) {
        throw std::runtime_error("Expected 3D buffer with shape [height, width, 4]");
    }

    int height = info.shape[0];
    int width = info.shape[1];
    int channels = info.shape[2];

    auto *ptr = static_cast<std::uint8_t *>(info.ptr);
    std::vector<std::uint8_t> flatPixels(ptr, ptr + width * height * channels);
    img.update(flatPixels.data());
}

void ApplyACBinding(py::module &m) {
    m.def("C_ImageUpdateBuffer1D", &C_ImageUpdateBuffer1D,
          "Update a texture from a flat RGBA byte buffer.",
          py::arg("img"), py::arg("buffer"));
    m.def("C_ImageUpdateBuffer3D", &C_ImageUpdateBuffer3D,
          "Update a texture from a [height, width, 4] RGBA byte buffer.",
          py::arg("img"), py::arg("buffer"));
}
