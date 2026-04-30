#include <pybind11/stl.h>

#include <GameMap.hpp>

void ApplyGameMapBinding(py::module &m) {
    py::class_<GameMapExt> GameMapExtClass(m, "GameMapExt");
    GameMapExtClass.def(py::init<sf::Shader *>(), py::arg("shader"));
    GameMapExtClass.def("refreshShader", &GameMapExt::refreshShader, py::arg("lightMask"), py::arg("screenScale"),
                        py::arg("screenSize"), py::arg("viewPos"), py::arg("viewRot"), py::arg("gridSize"),
                        py::arg("cellSize"), py::arg("lights"), py::arg("ambientColor"));
    GameMapExtClass.def("generateDataFromMap", &GameMapExt::generateDataFromMap, py::arg("size"),
                        py::arg("materialMap"), py::arg("smooth") = false);
    GameMapExtClass.def("findPathExt", &GameMapExt::findPathExt, py::arg("start"), py::arg("goal"), py::arg("size"));
    GameMapExtClass.def("getMaterialPropertyMapExt", &GameMapExt::getMaterialPropertyMapExt, py::arg("width"),
                        py::arg("height"), py::arg("functionName"), py::arg("invalidValue"));
    GameMapExtClass.def("rebuildPassabilityCache", &GameMapExt::rebuildPassabilityCache, py::arg("size"));
    GameMapExtClass.def_readwrite("tilemapRef", &GameMapExt::tilemapRef);
    GameMapExtClass.def_readwrite("layerKeysRef", &GameMapExt::layerKeysRef);
    GameMapExtClass.def_readwrite("actorsRef", &GameMapExt::actorsRef);
    GameMapExtClass.def_readwrite("tileDataRef", &GameMapExt::tileDataRef);
    GameMapExtClass.def_readwrite("getLayer", &GameMapExt::getLayer);
    GameMapExtClass.def_readwrite("getMapPosition", &GameMapExt::getMapPosition);
    GameMapExtClass.def_readwrite("getCollisionEnabled", &GameMapExt::getCollisionEnabled);
    GameMapExtClass.def_readwrite("TileLayerPassable", &GameMapExt::TileLayerPassable);
}
