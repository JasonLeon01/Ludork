#pragma once

#include <pybind11/pybind11.h>

#include "GameMapExt.hpp"

namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Register `GameMapExt` Python bindings
///
/// - \param m Target Python module
///
////////////////////////////////////////////////////////////
void ApplyGameMapBinding(py::module &m);
