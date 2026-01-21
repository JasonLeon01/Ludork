#include <GameMap/GetLightMap.h>
#include <stdexcept>
#include <utils.h>
#include <vector>

static double getLightBlock(std::vector<PyObject *> inLayerKeys, PyObject *pos,
                            PyObject *tilemap, PyObject *actors) {
  std::vector<PyObject *> tempPyObjects;
  try {
    for (auto &layerName : inLayerKeys) {
      PyObject *layer = GetMethodResult(tilemap, "getLayer", {layerName});
      CHECK_NULL(tempPyObjects, layer, "Failed to call getLayer method");
      tempPyObjects.push_back(layer);
      bool visible = GetAttrBool(layer, "visible", false);
      if (!visible) {
        continue;
      }
      int layerInActors = PyDict_Contains(actors, layerName);
      if (layerInActors == -1) {
        ClearCache(tempPyObjects);
        throw std::runtime_error("Failed to call PyDict_Contains method");
      }
      if (layerInActors) {
        PyObject *actorsListObj = PyDict_GetItem(actors, layerName);
        CHECK_NULL(tempPyObjects, actorsListObj,
                   "Failed to call PyDict_GetItem method");
        auto actorsList = GetPyListItems(actorsListObj);
        if (!actorsList.has_value()) {
          ClearCache(tempPyObjects);
          throw std::runtime_error("Failed to get actors list");
        }
        for (auto &actor : actorsList.value()) {
          PyObject *actorMapPos = GetMethodResult(actor, "getMapPosition", {});
          CHECK_NULL(tempPyObjects, actorMapPos,
                     "Failed to call getMapPosition method");
          tempPyObjects.push_back(actorMapPos);
          int eq = PyObject_RichCompareBool(actorMapPos, pos, Py_EQ);
          if (eq == -1) {
            ClearCache(tempPyObjects);
            throw std::runtime_error("Failed to compare actorMapPos with pos");
          }
          if (eq) {
            PyObject *lightBlockObj =
                GetMethodResult(actor, "getLightBlock", {});
            CHECK_NULL(tempPyObjects, lightBlockObj,
                       "Failed to call getLightBlock method");
            tempPyObjects.push_back(lightBlockObj);
            double lightBlock = PyFloat_AsDouble(lightBlockObj);
            if (PyErr_Occurred()) {
              ClearCache(tempPyObjects);
              throw std::runtime_error("Failed to read actor light block");
            }
            ClearCache(tempPyObjects);
            return lightBlock;
          }
        }
      }
      PyObject *tile = GetMethodResult(layer, "get", {pos});
      CHECK_NULL(tempPyObjects, tile, "Failed to call get method");
      tempPyObjects.push_back(tile);
      if (tile != Py_None) {
        PyObject *lightBlockObj =
            GetMethodResult(layer, "getLightBlock", {pos});
        CHECK_NULL(tempPyObjects, lightBlockObj,
                   "Failed to call getLightBlock method");
        tempPyObjects.push_back(lightBlockObj);
        double lightBlock = PyFloat_AsDouble(lightBlockObj);
        if (PyErr_Occurred()) {
          ClearCache(tempPyObjects);
          throw std::runtime_error("Failed to read layer light block");
        }
        ClearCache(tempPyObjects);
        return lightBlock;
      }
    }
    ClearCache(tempPyObjects);
    return 0;
  } catch (const std::exception &) {
    ClearCache(tempPyObjects);
    throw;
  }
}

PyObject *C_GetLightMap(PyObject *self, PyObject *args) {
  std::vector<PyObject *> tempPyObjects;
  try {
    PyObject *layerKeysObj;
    int width;
    int height;
    PyObject *tilemap;
    PyObject *actors;
    if (!PyArg_ParseTuple(args, "OiiOO", &layerKeysObj, &width, &height,
                          &tilemap, &actors)) {
      return nullptr;
    }
    PyObject *engineModule = PyImport_ImportModule("Engine");
    CHECK_NULL(tempPyObjects, engineModule, "Failed to import Engine module");
    tempPyObjects.push_back(engineModule);
    PyObject *Vector2iClass = PyObject_GetAttrString(engineModule, "Vector2i");
    CHECK_NULL(tempPyObjects, Vector2iClass,
               "Failed to get Vector2i class from Engine module");
    tempPyObjects.push_back(Vector2iClass);
    auto layerKeys = GetPyListItems(layerKeysObj);
    if (!layerKeys.has_value()) {
      ClearCache(tempPyObjects);
      throw std::runtime_error("Failed to get layerKeys from layerKeysObj");
    }
    std::vector<PyObject *> lightMap(height);
    for (int y = 0; y < height; ++y) {
      std::vector<double> rowLightBlock(width);
      for (int x = 0; x < width; ++x) {
        PyObject *pos = PyObject_CallFunction(Vector2iClass, "ii", x, y);
        if (!pos) {
          PyErr_Clear();
          pos = PyObject_CallFunction(Vector2iClass, "ii", x, y);
        }
        CHECK_NULL(tempPyObjects, pos, "Failed to create position");
        tempPyObjects.push_back(pos);
        rowLightBlock[x] =
            getLightBlock(layerKeys.value(), pos, tilemap, actors);
      }
      auto rowLightBlockVec = FromVectorFloatToPyList(rowLightBlock);
      PyObject *rowLightBlockObj = FromVectorPyObjToPyList(rowLightBlockVec);
      CHECK_NULL(tempPyObjects, rowLightBlockObj,
                 "Failed to create rowLightBlockObj");
      tempPyObjects.push_back(rowLightBlockObj);
      lightMap[y] = rowLightBlockObj;
    }
    PyObject *lightMapObj = FromVectorPyObjToPyList(lightMap);
    CHECK_NULL(tempPyObjects, lightMapObj, "Failed to create lightMapObj");
    ClearCache(tempPyObjects);
    return lightMapObj;
  } catch (const std::exception &e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return nullptr;
  }
}
