#include <Tilemap.h>
#include <GameMap.h>

static PyMethodDef GamePlayExtensionMethods[] = {
    {"calculateVertexArray", calculateVertexArray, METH_VARARGS},
    {"getLightMap", getLightMap, METH_VARARGS},
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
