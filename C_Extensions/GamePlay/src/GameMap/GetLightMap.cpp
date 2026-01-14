#include <utils.h>
#include <GameMap/GetLightMap.h>

PyObject* getLightMap(PyObject* self, PyObject* args) {
    PyObject *gameMapObj;
    if (!PyArg_ParseTuple(args, "O", &gameMapObj)) {
        return NULL;
    }

    PyObject *tilemap = PyObject_GetAttrString(gameMapObj, "_tilemap");
    if (!tilemap) {
        return NULL;
    }

    PyObject *mapSize = PyObject_CallMethod(tilemap, "getSize", NULL);
    if (!mapSize) { 
        Py_DECREF(tilemap); 
        return NULL; 
    }
    long width = getAttrLong(mapSize, "x", 0);
    long height = getAttrLong(mapSize, "y", 0);
    Py_DECREF(mapSize);

    PyObject *allLayers = PyObject_CallMethod(tilemap, "getAllLayers", NULL);
    if (!allLayers) { 
        Py_DECREF(tilemap); 
        return NULL; 
    }
    PyObject *keysView = PyMapping_Keys(allLayers);
    Py_DECREF(allLayers);
    if (!keysView) { 
        Py_DECREF(tilemap); 
        return NULL; 
    }
    
    if (PyList_Reverse(keysView) < 0) {
        Py_DECREF(keysView); 
        Py_DECREF(tilemap); 
        return NULL;
    }

    PyObject *activeLayer = NULL;
    PyObject *activeLayerName = NULL;
    Py_ssize_t numLayers = PyList_Size(keysView);
    for (Py_ssize_t i = 0; i < numLayers; ++i) {
        PyObject *key = PyList_GetItem(keysView, i);
        PyObject *layer = PyObject_CallMethod(tilemap, "getLayer", "O", key);
        if (layer) {
            bool isVisible = getAttrBool(layer, "visible", false);
            if (isVisible) {
                activeLayer = layer;
                activeLayerName = key;
                Py_INCREF(activeLayerName);
                break;
            }
            Py_DECREF(layer);
        }
    }
    Py_DECREF(keysView);
    Py_DECREF(tilemap);

    if (!activeLayer) {
        PyObject *result = PyList_New(height);
        for (long y = 0; y < height; ++y) {
            PyObject *row = PyList_New(width);
            for (long x = 0; x < width; ++x) {
                PyList_SET_ITEM(row, x, PyFloat_FromDouble(0.0));
            }
            PyList_SET_ITEM(result, y, row);
        }
        return result;
    }

    PyObject *layerData = PyObject_GetAttrString(activeLayer, "_data");
    PyObject *tiles = NULL;
    PyObject *tileset = NULL;
    PyObject *passable = NULL;
    PyObject *lightBlockList = NULL;

    if (layerData) {
        tiles = PyObject_GetAttrString(layerData, "tiles");
        tileset = PyObject_GetAttrString(layerData, "layerTileset");
        if (tileset) {
            passable = PyObject_GetAttrString(tileset, "passable");
            lightBlockList = PyObject_GetAttrString(tileset, "lightBlock");
            Py_DECREF(tileset);
        }
        Py_DECREF(layerData);
    }
    
    if (!tiles || !passable || !lightBlockList) {
        Py_XDECREF(tiles); Py_XDECREF(passable); Py_XDECREF(lightBlockList);
        Py_DECREF(activeLayer); Py_DECREF(activeLayerName);
        PyErr_SetString(PyExc_RuntimeError, "Failed to retrieve layer data for C extension");
        return NULL;
    }

    double *actorGrid = (double*)malloc(width * height * sizeof(double));
    if (!actorGrid) {
        Py_DECREF(tiles); Py_DECREF(passable); Py_DECREF(lightBlockList);
        Py_DECREF(activeLayer); Py_DECREF(activeLayerName);
        return PyErr_NoMemory();
    }
    for (long i=0; i<width*height; ++i) {
        actorGrid[i] = -1.0;
    }

    PyObject *actorsDict = PyObject_GetAttrString(gameMapObj, "_actors");
    if (actorsDict) {
        PyObject *actorList = PyDict_GetItem(actorsDict, activeLayerName);
        if (actorList && PyList_Check(actorList)) {
            Py_ssize_t numActors = PyList_Size(actorList);
            for (Py_ssize_t i = 0; i < numActors; ++i) {
                PyObject *actor = PyList_GetItem(actorList, i);
                bool is_coll = isMethodTrue(actor, "getCollisionEnabled");
                double val = 0.0;
                if (is_coll) {
                    PyObject *lbObj = PyObject_CallMethod(actor, "getLightBlock", NULL);
                    if (lbObj) {
                        val = PyFloat_AsDouble(lbObj);
                        Py_DECREF(lbObj);
                    }
                } else {
                    val = 0.0;
                }

                PyObject *posObj = PyObject_CallMethod(actor, "getMapPosition", NULL);
                if (posObj) {
                    long ax = getAttrLong(posObj, "x", -1);
                    long ay = getAttrLong(posObj, "y", -1);
                    Py_DECREF(posObj);

                    if (ax >= 0 && ax < width && ay >= 0 && ay < height) {
                        long idx = ay * width + ax;
                        if (actorGrid[idx] < -0.5) {
                            actorGrid[idx] = val;
                        }
                    }
                }
            }
        }
        Py_DECREF(actorsDict);
    }

    PyObject *result = PyList_New(height);
    for (long y = 0; y < height; ++y) {
        PyObject *row = PyList_New(width);
        PyObject *tileRow = PyList_GetItem(tiles, y);
        
        for (long x = 0; x < width; ++x) {
            double finalVal = 0.0;
            long idx = y * width + x;

            if (actorGrid[idx] >= -0.5) {
                finalVal = actorGrid[idx];
            } else {
                if (tileRow) {
                    PyObject *tileNumObj = PyList_GetItem(tileRow, x);
                    if (tileNumObj == Py_None) {
                        finalVal = 0.0;
                    } else {
                        long tileNum = PyLong_AsLong(tileNumObj);
                        PyObject *isPass = PyList_GetItem(passable, tileNum);
                        if (PyObject_IsTrue(isPass)) {
                            finalVal = 0.0;
                        } else {
                            PyObject *lb = PyList_GetItem(lightBlockList, tileNum);
                            finalVal = PyFloat_AsDouble(lb);
                        }
                    }
                }
            }
            PyList_SET_ITEM(row, x, PyFloat_FromDouble(finalVal));
        }
        PyList_SET_ITEM(result, y, row);
    }

    free(actorGrid);
    Py_DECREF(activeLayer);
    Py_DECREF(activeLayerName);
    Py_DECREF(tiles);
    Py_DECREF(passable);
    Py_DECREF(lightBlockList);

    return result;
}
