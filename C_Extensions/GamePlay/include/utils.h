#pragma once

#include <Python.h>
#include <utility>

typedef std::pair<int, int> IntPair;

long getAttrLong(PyObject* obj, const char* attr_name, long default_val);
bool getAttrBool(PyObject* obj, const char* attr_name, bool default_val);
bool isMethodTrue(PyObject* obj, const char* method_name);
