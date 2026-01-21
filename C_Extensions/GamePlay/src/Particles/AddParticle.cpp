#include <Particles/AddParticle.h>
#include <stdexcept>
#include <utils.h>
#include <vector>


PyObject *C_AddParticle(PyObject *self, PyObject *args) {
  std::vector<PyObject *> tempPyObjects;
  try {
    PyObject *engineModule = PyImport_ImportModule("Engine");
    CHECK_NULL(tempPyObjects, engineModule, "Failed to import Engine module");
    tempPyObjects.push_back(engineModule);
    PyObject *transformClass =
        PyObject_GetAttrString(engineModule, "Transform");
    CHECK_NULL(tempPyObjects, transformClass, "Failed to get Transform class");
    tempPyObjects.push_back(transformClass);
    PyObject *vertexClass = PyObject_GetAttrString(engineModule, "Vertex");
    CHECK_NULL(tempPyObjects, vertexClass, "Failed to get Vertex class");
    tempPyObjects.push_back(vertexClass);
    PyObject *info;
    PyObject *uv_tl;
    PyObject *uv_tr;
    PyObject *uv_br;
    PyObject *uv_bl;
    PyObject *tl_tr;
    PyObject *tr_tr;
    PyObject *br_tr;
    PyObject *bl_tr;
    PyObject *vertexArray;
    if (!PyArg_ParseTuple(args, "OOOOOOOOOO", &info, &uv_tl, &uv_tr, &uv_br,
                          &uv_bl, &tl_tr, &tr_tr, &br_tr, &bl_tr,
                          &vertexArray)) {
      ClearCache(tempPyObjects);
      throw std::runtime_error("Failed to parse arguments");
    }
    PyObject *t = PyObject_CallObject(transformClass, NULL);
    CHECK_NULL(tempPyObjects, t, "Failed to create Transform instance");
    tempPyObjects.push_back(t);
    PyObject *position = PyObject_GetAttrString(info, "position");
    CHECK_NULL(tempPyObjects, position, "Failed to get position attribute");
    tempPyObjects.push_back(position);
    DoMethod(t, "translate", {position});
    RemoveObj(tempPyObjects, position);
    PyObject *rotation = PyObject_GetAttrString(info, "rotation");
    CHECK_NULL(tempPyObjects, rotation, "Failed to get rotation attribute");
    tempPyObjects.push_back(rotation);
    DoMethod(t, "rotate", {rotation});
    RemoveObj(tempPyObjects, rotation);
    PyObject *scale = PyObject_GetAttrString(info, "scale");
    CHECK_NULL(tempPyObjects, scale, "Failed to get scale attribute");
    tempPyObjects.push_back(scale);
    DoMethod(t, "scale", {scale});
    RemoveObj(tempPyObjects, scale);
    PyObject *tl = GetMethodResult(t, "transformPoint", {tl_tr});
    CHECK_NULL(tempPyObjects, tl, "Failed to transformPoint tl_tr");
    tempPyObjects.push_back(tl);
    PyObject *tr = GetMethodResult(t, "transformPoint", {tr_tr});
    CHECK_NULL(tempPyObjects, tr, "Failed to transformPoint tr_tr");
    tempPyObjects.push_back(tr);
    PyObject *br = GetMethodResult(t, "transformPoint", {br_tr});
    CHECK_NULL(tempPyObjects, br, "Failed to transformPoint br_tr");
    tempPyObjects.push_back(br);
    PyObject *bl = GetMethodResult(t, "transformPoint", {bl_tr});
    CHECK_NULL(tempPyObjects, bl, "Failed to transformPoint bl_tr");
    tempPyObjects.push_back(bl);
    std::vector<PyObject *> positions = {tl, tr, br, tl, br, bl};
    std::vector<PyObject *> uvs = {uv_tl, uv_tr, uv_br, uv_tl, uv_br, uv_bl};
    for (int i = 0; i < 6; ++i) {
      PyObject *v = PyObject_CallObject(vertexClass, NULL);
      CHECK_NULL(tempPyObjects, v, "Failed to create Vertex instance");
      tempPyObjects.push_back(v);
      int r = PyObject_SetAttrString(v, "position", positions[i]);
      if (r != 0) {
        ClearCache(tempPyObjects);
        throw std::runtime_error("Failed to set position attribute");
      }
      r = PyObject_SetAttrString(v, "texCoords", uvs[i]);
      if (r != 0) {
        ClearCache(tempPyObjects);
        throw std::runtime_error("Failed to set texCoords attribute");
      }
      PyObject *color = PyObject_GetAttrString(info, "color");
      CHECK_NULL(tempPyObjects, color, "Failed to get color attribute");
      r = PyObject_SetAttrString(v, "color", color);
      Py_DECREF(color);
      if (r != 0) {
        ClearCache(tempPyObjects);
        throw std::runtime_error("Failed to set color attribute");
      }
      DoMethod(vertexArray, "append", {v});
    }
    ClearCache(tempPyObjects);
    Py_RETURN_NONE;
  } catch (const std::exception &e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    ClearCache(tempPyObjects);
    return nullptr;
  }
}
