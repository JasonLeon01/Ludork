#include <cstddef>
#include <stdexcept>
#include <utils.h>

long GetAttrLong(PyObject *obj, const char *attrName, long defaultVal) {
  if (!obj)
    return defaultVal;
  PyObject *attr = PyObject_GetAttrString(obj, attrName);
  if (!attr) {
    PyErr_Clear();
    return defaultVal;
  }
  long val = PyLong_AsLong(attr);
  if (PyErr_Occurred()) {
    PyErr_Clear();
    val = defaultVal;
  }
  Py_DECREF(attr);
  return val;
}

bool GetAttrBool(PyObject *obj, const char *attrName, bool defaultVal) {
  if (!obj)
    return defaultVal;
  PyObject *attr = PyObject_GetAttrString(obj, attrName);
  if (!attr) {
    PyErr_Clear();
    return defaultVal;
  }
  bool val = PyObject_IsTrue(attr);
  if (PyErr_Occurred()) {
    PyErr_Clear();
    val = defaultVal;
  }
  Py_DECREF(attr);
  return val;
}

double GetAttrFloat(PyObject *obj, const char *attrName, double defaultVal) {
  if (!obj)
    return defaultVal;
  PyObject *attr = PyObject_GetAttrString(obj, attrName);
  if (!attr) {
    PyErr_Clear();
    return defaultVal;
  }
  double val = PyFloat_AsDouble(attr);
  if (PyErr_Occurred()) {
    PyErr_Clear();
    val = defaultVal;
  }
  Py_DECREF(attr);
  return val;
}

std::string ToString(PyObject *obj) {
  if (!obj) {
    throw std::runtime_error("Object is NULL");
    return "";
  }
  PyObject *str = PyObject_Str(obj);
  if (!str) {
    throw std::runtime_error("Failed to convert object to string");
    return "";
  }
  std::string result = PyUnicode_AsUTF8(str);
  Py_DECREF(str);
  return result;
}

PyObject *NewInstance(PyObject *classObj, std::vector<PyObject *> params) {
  PyObject *args = PyTuple_New(params.size());
  if (!args) {
    return nullptr;
  }
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (!params[i]) {
      PyErr_SetString(PyExc_TypeError, "Argument is NULL");
      Py_DECREF(args);
      return nullptr;
    }
    Py_INCREF(params[i]);
    PyTuple_SetItem(args, i, params[i]);
  }
  PyObject *instance = PyObject_CallObject(classObj, args);
  Py_DECREF(args);
  if (!instance) {
    return nullptr;
  }
  return instance;
}

PyObject *GetMethodResult(PyObject *obj, const char *methodName,
                          std::vector<PyObject *> params) {
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, "Object is NULL");
    return nullptr;
  }
  if (!PyObject_IsInstance(obj, (PyObject *)&PyBaseObject_Type)) {
    PyErr_SetString(PyExc_TypeError, "Object is not a base object");
    return nullptr;
  }
  if (methodName == nullptr || methodName[0] == '\0') {
    PyErr_SetString(PyExc_TypeError, "Method name is empty");
    return nullptr;
  }
  PyObject *method = PyObject_GetAttrString(obj, methodName);
  if (!method) {
    return nullptr;
  }
  PyObject *args = PyTuple_New(params.size());
  if (!args) {
    Py_DECREF(method);
    return nullptr;
  }
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (!params[i]) {
      PyErr_SetString(PyExc_TypeError, "Argument is NULL");
      Py_DECREF(method);
      Py_DECREF(args);
      return nullptr;
    }
    Py_INCREF(params[i]);
    PyTuple_SetItem(args, i, params[i]);
  }
  PyObject *result = PyObject_Call(method, args, NULL);
  Py_DECREF(method);
  Py_DECREF(args);
  if (!result) {
    return nullptr;
  }
  return result;
}

void DoMethod(PyObject *obj, const char *methodName,
              std::vector<PyObject *> params) {
  PyObject *result = GetMethodResult(obj, methodName, params);
  if (!result) {
    return;
  }
  Py_DECREF(result);
}

bool IsMethodTrue(PyObject *obj, const char *methodName,
                  std::vector<PyObject *> params) {
  PyObject *result = GetMethodResult(obj, methodName, params);
  if (!result) {
    return false;
  }
  bool val = PyObject_IsTrue(result);
  if (PyErr_Occurred()) {
    PyErr_Clear();
    val = false;
  }
  Py_DECREF(result);
  return val;
}

void ClearCache(std::vector<PyObject *> &tempPyObjects) {
  for (auto &obj : tempPyObjects) {
    Py_DECREF(obj);
  }
  tempPyObjects.clear();
};

void RemoveObj(std::vector<PyObject *> &tempPyObjects, PyObject *obj) {
  tempPyObjects.erase(
      std::remove(tempPyObjects.begin(), tempPyObjects.end(), obj),
      tempPyObjects.end());
  Py_DECREF(obj);
};

std::optional<std::vector<PyObject *>> GetPyListItems(PyObject *list) {
  if (!PyList_Check(list)) {
    PyErr_SetString(PyExc_TypeError, "Argument must be a list");
    return std::nullopt;
  }
  Py_ssize_t len = PyList_Size(list);
  if (len < 0) {
    PyErr_SetString(PyExc_TypeError, "List size is negative");
    return std::nullopt;
  }
  std::vector<PyObject *> items(len);
  for (Py_ssize_t i = 0; i < len; ++i) {
    PyObject *item = PyList_GetItem(list, i);
    if (item == NULL) {
      PyErr_SetString(PyExc_TypeError, "List item is NULL");
      return std::nullopt;
    }
    items[i] = item;
  }
  return items;
}

std::vector<PyObject *> FromVectorFloatToPyList(std::vector<double> &lightMap) {
  std::vector<PyObject *> pyObjs(lightMap.size());
  for (std::size_t i = 0; i < lightMap.size(); ++i) {
    pyObjs[i] = PyFloat_FromDouble(lightMap[i]);
  }
  return pyObjs;
}

PyObject *FromVectorPyObjToPyList(std::vector<PyObject *> &pyObjs) {
  PyObject *list = PyList_New(pyObjs.size());
  if (!list) {
    return nullptr;
  }
  for (std::size_t i = 0; i < pyObjs.size(); ++i) {
    Py_INCREF(pyObjs[i]);
    PyList_SetItem(list, i, pyObjs[i]);
  }
  return list;
}

PyObject *GetVertex(PyObject *vertexArray, Py_ssize_t index) {
  PyObject *tempIndexObj = PyLong_FromLong(index);
  PyObject *vertexObj =
      GetMethodResult(vertexArray, "__getitem__", {tempIndexObj});
  Py_DECREF(tempIndexObj);
  if (!vertexObj) {
    PyErr_SetString(PyExc_TypeError, "Failed to get vertex");
    return nullptr;
  }
  return vertexObj;
}

void CHECK_NULL(std::vector<PyObject *> &tempPyObjects, PyObject *obj,
                const char *msg) {
  if (!obj) {
    ClearCache(tempPyObjects);
    throw std::runtime_error(msg);
  }
}
