#pragma once

#include <GameMap/FindPath.h>
#include <GameMap/GetMaterialPropertyMap.h>
#include <GameMap/GetMaterialPropertyTexture.h>
#include <GameMap/RebuildPassabilityCache.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

void ApplyGameMapBinding(py::module &m);
