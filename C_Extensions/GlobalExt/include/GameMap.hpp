#pragma once

#include <pybind11/pybind11.h>

#include "GameMapExt.hpp"

namespace py = pybind11;

void ApplyGameMapBinding(py::module &m);
