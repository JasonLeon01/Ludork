#include <Particles/UpdateParticlesInfo.h>
#include <stdexcept>
#include <utils.h>
#include <vector>

PyObject *C_UpdateParticlesInfo(PyObject *self, PyObject *args) {
  std::vector<PyObject *> tempPyObjects;
  try {
    PyObject *engineModule = PyImport_ImportModule("Engine");
    CHECK_NULL(tempPyObjects, engineModule, "Failed to import Engine module");
    tempPyObjects.push_back(engineModule);
    PyObject *transformClass =
        PyObject_GetAttrString(engineModule, "Transform");
    CHECK_NULL(tempPyObjects, transformClass, "Failed to get Transform class");
    tempPyObjects.push_back(transformClass);
    PyObject *getUpdateParticleInfo;
    PyObject *updateFlags;
    PyObject *particles;
    PyObject *vertexArrays;
    if (!PyArg_ParseTuple(args, "OOOO", &getUpdateParticleInfo, &updateFlags,
                          &particles, &vertexArrays)) {
      ClearCache(tempPyObjects);
      throw std::runtime_error("Failed to parse arguments");
    }
    auto C_UpdateFlags = GetPyListItems(updateFlags);
    if (!C_UpdateFlags.has_value()) {
      ClearCache(tempPyObjects);
      throw std::runtime_error("Failed to parse updateFlags");
    }
    for (auto &particle : C_UpdateFlags.value()) {
      PyObject *resourcePathObj =
          PyObject_GetAttrString(particle, "resourcePath");
      CHECK_NULL(tempPyObjects, resourcePathObj,
                 "Failed to get resourcePath attribute");
      tempPyObjects.push_back(resourcePathObj);
      PyObject *particlesListObj = PyDict_GetItem(particles, resourcePathObj);
      CHECK_NULL(tempPyObjects, particlesListObj,
                 "Failed to get particle list for resourcePath");
      Py_INCREF(particlesListObj);
      tempPyObjects.push_back(particlesListObj);
      PyObject *indexObj =
          GetMethodResult(particlesListObj, "index", {particle});
      CHECK_NULL(tempPyObjects, indexObj, "Failed to get index of particle");
      tempPyObjects.push_back(indexObj);
      int index = PyLong_AsLong(indexObj);
      RemoveObj(tempPyObjects, indexObj);

      PyObject *t = PyObject_CallObject(transformClass, NULL);
      CHECK_NULL(tempPyObjects, t, "Failed to create Transform instance");
      tempPyObjects.push_back(t);
      PyObject *infoObj = PyObject_GetAttrString(particle, "info");
      CHECK_NULL(tempPyObjects, infoObj, "Failed to get info attribute");
      tempPyObjects.push_back(infoObj);
      PyObject *positionObj = PyObject_GetAttrString(infoObj, "position");
      CHECK_NULL(tempPyObjects, positionObj,
                 "Failed to get position attribute");
      tempPyObjects.push_back(positionObj);
      PyObject *rotationObj = PyObject_GetAttrString(infoObj, "rotation");
      CHECK_NULL(tempPyObjects, rotationObj,
                 "Failed to get rotation attribute");
      tempPyObjects.push_back(rotationObj);
      PyObject *scaleObj = PyObject_GetAttrString(infoObj, "scale");
      CHECK_NULL(tempPyObjects, scaleObj, "Failed to get scale attribute");
      tempPyObjects.push_back(scaleObj);
      DoMethod(t, "translate", {positionObj});
      DoMethod(t, "rotate", {rotationObj});
      DoMethod(t, "scale", {scaleObj});

      RemoveObj(tempPyObjects, positionObj);
      RemoveObj(tempPyObjects, rotationObj);
      RemoveObj(tempPyObjects, scaleObj);

      PyObject *fArgTuple = PyTuple_New(1);
      CHECK_NULL(tempPyObjects, fArgTuple, "Failed to create argument tuple");
      tempPyObjects.push_back(fArgTuple);
      Py_INCREF(particle);
      PyTuple_SetItem(fArgTuple, 0, particle);
      PyObject *trInfo = PyObject_CallObject(getUpdateParticleInfo, fArgTuple);
      CHECK_NULL(tempPyObjects, trInfo, "Failed to get transform info");
      tempPyObjects.push_back(trInfo);
      PyObject *tl_tr, *tr_tr, *br_tr, *bl_tr;
      if (!PyArg_ParseTuple(trInfo, "OOOO", &tl_tr, &tr_tr, &br_tr, &bl_tr)) {
        ClearCache(tempPyObjects);
        throw std::runtime_error("Failed to parse transform info");
      }
      PyObject *tl = GetMethodResult(t, "transformPoint", {tl_tr});
      CHECK_NULL(tempPyObjects, tl, "Failed to transform tl_tr");
      tempPyObjects.push_back(tl);
      PyObject *tr = GetMethodResult(t, "transformPoint", {tr_tr});
      CHECK_NULL(tempPyObjects, tr, "Failed to transform tr_tr");
      tempPyObjects.push_back(tr);
      PyObject *br = GetMethodResult(t, "transformPoint", {br_tr});
      CHECK_NULL(tempPyObjects, br, "Failed to transform br_tr");
      tempPyObjects.push_back(br);
      PyObject *bl = GetMethodResult(t, "transformPoint", {bl_tr});
      CHECK_NULL(tempPyObjects, bl, "Failed to transform bl_tr");
      tempPyObjects.push_back(bl);
      RemoveObj(tempPyObjects, trInfo);

      PyObject *vertexArrayObj = PyDict_GetItem(vertexArrays, resourcePathObj);
      CHECK_NULL(tempPyObjects, vertexArrayObj,
                 "Failed to get vertex array for resourcePath");
      std::vector<PyObject *> positions = {tl, tr, br, tl, br, bl};
      PyObject *colorObj = PyObject_GetAttrString(infoObj, "color");
      CHECK_NULL(tempPyObjects, colorObj, "Failed to get color attribute");
      tempPyObjects.push_back(colorObj);
      for (int i = 0; i < 6; ++i) {
        PyObject *vertexObj = GetVertex(vertexArrayObj, index * 6 + i);
        CHECK_NULL(tempPyObjects, vertexObj, "Failed to get vertex");
        tempPyObjects.push_back(vertexObj);
        PyObject_SetAttrString(vertexObj, "position", positions[i]);
        PyObject_SetAttrString(vertexObj, "color", colorObj);
        RemoveObj(tempPyObjects, vertexObj);
      }
    }
    ClearCache(tempPyObjects);
    Py_RETURN_NONE;
  } catch (const std::exception &e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    ClearCache(tempPyObjects);
    return nullptr;
  }
}
