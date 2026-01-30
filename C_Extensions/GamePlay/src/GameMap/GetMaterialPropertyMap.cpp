#include <GameMap/GetMaterialPropertyMap.h>
#include <stdexcept>
#include <utils.h>
#include <vector>

static PyObject *getMaterialProperty(const std::vector<PyObject *> &inLayerKeys,
                                     PyObject *pos, PyObject *tilemap,
                                     PyObject *actors,
                                     const std::string &functionName,
                                     PyObject *invalidValue) {
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
            PyObject *materialPropertyObj =
                GetMethodResult(actor, functionName.c_str(), {});
            int eqValid = PyObject_RichCompareBool(materialPropertyObj,
                                                   invalidValue, Py_EQ);
            if (eqValid == -1) {
              ClearCache(tempPyObjects);
              throw std::runtime_error(
                  "Failed to compare materialPropertyObj with invalidValue");
            }
            if (eqValid) {
              tempPyObjects.push_back(materialPropertyObj);
              continue;
            }
            CHECK_NULL(tempPyObjects, materialPropertyObj,
                       ("Failed to call " + functionName + " method").c_str());
            ClearCache(tempPyObjects);
            return materialPropertyObj;
          }
        }
      }
      PyObject *tile = GetMethodResult(layer, "get", {pos});
      CHECK_NULL(tempPyObjects, tile, "Failed to call get method");
      tempPyObjects.push_back(tile);
      if (tile != Py_None) {
        PyObject *materialPropertyObj =
            GetMethodResult(layer, functionName.c_str(), {pos});
        CHECK_NULL(tempPyObjects, materialPropertyObj,
                   ("Failed to call " + functionName + " method").c_str());
        if (PyErr_Occurred()) {
          ClearCache(tempPyObjects);
          throw std::runtime_error("Failed to read layer light block");
        }
        ClearCache(tempPyObjects);
        return materialPropertyObj;
      }
    }
    ClearCache(tempPyObjects);
    Py_INCREF(invalidValue);
    return invalidValue;
  } catch (const std::exception &) {
    ClearCache(tempPyObjects);
    throw;
  }
}

PyObject *C_GetMaterialPropertyMap(PyObject *self, PyObject *args) {
  std::vector<PyObject *> tempPyObjects;
  try {
    PyObject *layerKeysObj;
    int width;
    int height;
    PyObject *tilemap;
    PyObject *actors;
    PyObject *functionName;
    PyObject *invalidValue;
    if (!PyArg_ParseTuple(args, "OiiOOOO", &layerKeysObj, &width, &height,
                          &tilemap, &actors, &functionName, &invalidValue)) {
      return nullptr;
    }
    std::string functionNameStr = ToString(functionName);
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
      std::vector<PyObject *> rowMaterialProperty(width);
      for (int x = 0; x < width; ++x) {
        PyObject *pos =
            PyObject_CallFunction(Vector2iClass, "ii", x, height - y - 1);
        if (!pos) {
          ClearCache(tempPyObjects);
          throw std::runtime_error("Failed to create position");
        }
        CHECK_NULL(tempPyObjects, pos, "Failed to create position");
        tempPyObjects.push_back(pos);
        rowMaterialProperty[x] =
            getMaterialProperty(layerKeys.value(), pos, tilemap, actors,
                                functionNameStr, invalidValue);
      }
      PyObject *rowMaterialPropertyObj =
          FromVectorPyObjToPyList(rowMaterialProperty);
      CHECK_NULL(tempPyObjects, rowMaterialPropertyObj,
                 "Failed to create rowMaterialPropertyObj");
      tempPyObjects.push_back(rowMaterialPropertyObj);
      lightMap[y] = rowMaterialPropertyObj;
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
