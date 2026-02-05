#pragma once

#include <pybind11/pybind11.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace py = pybind11;

std::tuple<float, std::vector<py::bytes>, std::vector<py::dict>>
C_CompressAnimation(
    py::object zlibModule, int frameCount, float frameStep, int frameRate,
    const std::vector<std::unordered_map<std::string, std::vector<py::dict>>>
        &timeLines,
    const std::vector<std::string> &assets, const std::string &assetsRoot,
    const std::string &imageFormat);

void ApplyCompressAnimationBinding(py::module &m);
