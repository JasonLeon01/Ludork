#include <GameMap.h>
#include <Particle.h>
#include <Tilemap.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(GamePlayExtension, m) {
  m.def("C_CalculateVertexArray", &C_CalculateVertexArray,
        py::arg("vertexArray"), py::arg("tiles"), py::arg("tileSize"),
        py::arg("columns"), py::arg("width"), py::arg("height"));

  m.def("C_GetMaterialPropertyMap", &C_GetMaterialPropertyMap,
        py::arg("layerKeys"), py::arg("width"), py::arg("height"),
        py::arg("tilemap"), py::arg("actors"), py::arg("functionName"),
        py::arg("invalidValue"), py::arg("getLayer"), py::arg("getMapPosition"),
        py::arg("getTile"));
  m.def("C_GetMaterialPropertyTexture", &C_GetMaterialPropertyTexture,
        py::arg("size"), py::arg("img"), py::arg("materialMap"));
  m.def("C_FindPath", &C_FindPath, py::arg("start"), py::arg("goal"),
        py::arg("size"), py::arg("tilemap"), py::arg("layerKeys"),
        py::arg("actors"), py::arg("getLayer"), py::arg("getMapPosition"),
        py::arg("getCollisionEnabled"), py::arg("getTile"),
        py::arg("isPassable"));
  m.def("C_RebuildPassabilityCache", &C_RebuildPassabilityCache,
        py::arg("size"), py::arg("layerKeys"), py::arg("tileData"),
        py::arg("actors"), py::arg("tilemap"), py::arg("getLayer"),
        py::arg("isPassable"), py::arg("getCollisionEnabled"),
        py::arg("getMapPosition"));

  m.def("C_AddParticle", &C_AddParticle, py::arg("infoPosition"),
        py::arg("infoRotation"), py::arg("infoScale"), py::arg("infoColor"),
        py::arg("uv_tl"), py::arg("uv_tr"), py::arg("uv_br"), py::arg("uv_bl"),
        py::arg("tl_tr"), py::arg("tr_tr"), py::arg("br_tr"), py::arg("bl_tr"),
        py::arg("vertexArray"));
  m.def("C_UpdateParticlesInfo", &C_UpdateParticlesInfo,
        py::arg("getUpdateParticleInfo"), py::arg("updateFlags"),
        py::arg("particles"), py::arg("vertexArrays"));
  m.def("C_RemoveParticle", &C_RemoveParticle, py::arg("particles"),
        py::arg("vertexArrays"), py::arg("index"));
}