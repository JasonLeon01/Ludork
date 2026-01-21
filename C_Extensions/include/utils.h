#pragma once

#include <Python.h>
#include <vector>
#include <utility>
#include <optional>

typedef std::pair<int, int> IntPair;

long GetAttrLong(PyObject* obj, const char* attrName, long defaultVal);
bool GetAttrBool(PyObject* obj, const char* attrName, bool defaultVal);
double GetAttrFloat(PyObject* obj, const char* attrName, double defaultVal);
PyObject* NewInstance(PyObject *classObj, std::vector<PyObject*> params);
PyObject* GetMethodResult(PyObject* obj, const char* methodName, std::vector<PyObject*> params);
void DoMethod(PyObject* obj, const char* methodName, std::vector<PyObject*> params);
bool IsMethodTrue(PyObject* obj, const char* methodName, std::vector<PyObject*> params);
void ClearCache(std::vector<PyObject*>& tempPyObjects);
void RemoveObj(std::vector<PyObject*>& tempPyObjects, PyObject* obj);
std::optional<std::vector<PyObject*>> GetPyListItems(PyObject* list);
std::vector<PyObject*> FromVectorFloatToPyList(std::vector<double>& lightMap);
PyObject* FromVectorPyObjToPyList(std::vector<PyObject*>& pyObjs);
PyObject* GetVertex(PyObject* vertexArray, Py_ssize_t index);
void CHECK_NULL(std::vector<PyObject*>& tempPyObjects, PyObject* obj, const char* msg);
