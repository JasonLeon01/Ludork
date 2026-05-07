#include <pybind11/stl.h>

#include <GameMap.hpp>

void ApplyGameMapBinding(py::module &m) {
    py::class_<GameMapExt> GameMapExtClass(
        m, "GameMapExt", "Accelerated map helper for shader lighting and pathfinding.");
    GameMapExtClass.def(py::init<sf::Shader *>(), py::arg("shader"));
    GameMapExtClass.def("refreshShader", &GameMapExt::refreshShader,
                        "Refresh shader uniforms from camera and light data.",
                        py::arg("lightMask"), py::arg("screenScale"),
                        py::arg("screenSize"), py::arg("viewPos"), py::arg("viewRot"), py::arg("gridSize"),
                        py::arg("cellSize"), py::arg("lights"), py::arg("ambientColor"));
    GameMapExtClass.def("generateDataFromMap", &GameMapExt::generateDataFromMap,
                        "Generate a grayscale data texture from a material value map.",
                        py::arg("size"),
                        py::arg("materialMap"), py::arg("smooth") = false);
    GameMapExtClass.def("findPathExt", &GameMapExt::findPathExt,
                        "Run A* pathfinding and return movement deltas.",
                        py::arg("start"), py::arg("goal"), py::arg("size"));
    GameMapExtClass.def("getMaterialPropertyMapExt", &GameMapExt::getMaterialPropertyMapExt, py::arg("width"),
                        py::arg("height"), py::arg("functionName"), py::arg("invalidValue"),
                        "Build a 2D map of dynamic material properties.");
    GameMapExtClass.def("rebuildPassabilityCache", &GameMapExt::rebuildPassabilityCache,
                        "Rebuild tile passability and actor occupancy caches.",
                        py::arg("size"));
    GameMapExtClass.def_readwrite("tilemapRef", &GameMapExt::tilemapRef);
    GameMapExtClass.def_readwrite("layerKeysRef", &GameMapExt::layerKeysRef);
    GameMapExtClass.def_readwrite("actorsRef", &GameMapExt::actorsRef);
    GameMapExtClass.def_readwrite("tileDataRef", &GameMapExt::tileDataRef);
    GameMapExtClass.def_readwrite("getLayer", &GameMapExt::getLayer);
    GameMapExtClass.def_readwrite("getMapPosition", &GameMapExt::getMapPosition);
    GameMapExtClass.def_readwrite("getCollisionEnabled", &GameMapExt::getCollisionEnabled);
    GameMapExtClass.def_readwrite("TileLayerPassable", &GameMapExt::TileLayerPassable);
}
