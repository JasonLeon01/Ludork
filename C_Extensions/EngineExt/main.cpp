#define PYBIND11_INTERNALS_ID "PYSF"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iostream>

#include "Graphics/Color.hpp"
#include "Graphics/CompressAnimation.hpp"
#include "Graphics/RectBase.hpp"
#include "Graphics/TilemapGraphics.hpp"
#include "Particles.hpp"

namespace py = pybind11;

PYBIND11_MODULE(EngineExt, m) {
    void *internals_ptr = static_cast<void *>(const_cast<py::detail::internals *>(&py::detail::get_internals()));
    std::cout << "[Ludork PYSF Binding] PyBind11 internals address: " << internals_ptr << std::endl;

    auto importModule = py::module::import("importlib").attr("import_module");
    try {
        importModule("EngineExt.pysf");
    } catch (py::error_already_set &) {
        PyErr_Clear();
        importModule("pysf");
    }

    ApplyParticlesBinding(m);
    ApplyCompressAnimationBinding(m);
    ApplyRectBaseBinding(m);
    ApplyColorBinding(m);
    ApplyTileLayerGraphicsBinding(m);
}
