#include <Tilemap.h>
#include <GameMap.h>
#include <Particle.h>

static PyMethodDef GamePlayExtensionMethods[] = {
    {"C_CalculateVertexArray", C_CalculateVertexArray, METH_VARARGS},
    {"C_GetLightMap", C_GetLightMap, METH_VARARGS},
    {"C_FillPassabilityImage", C_FillPassabilityImage, METH_VARARGS},
    {"C_FindPath", C_FindPath, METH_VARARGS},
    {"C_AddParticle", C_AddParticle, METH_VARARGS},
    {"C_UpdateParticlesInfo", C_UpdateParticlesInfo, METH_VARARGS},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef GamePlayExtensionModule = {
    PyModuleDef_HEAD_INIT,
    "GamePlayExtension",
    NULL,
    -1,
    GamePlayExtensionMethods
};

PyMODINIT_FUNC PyInit_GamePlayExtension(void) {
    return PyModule_Create(&GamePlayExtensionModule);
}
