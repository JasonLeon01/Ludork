#pragma once

#include <Python.h>

#define coord_vec2_obj(objx, objy, x, y) PyObject *objx = PyFloat_FromDouble(x); PyObject *objy = PyFloat_FromDouble(y);
#define collect_vec(objx, objy, vec) Py_DECREF(objx); Py_DECREF(objy); Py_DECREF(vec);

PyObject* C_CalculateVertexArray(PyObject* self, PyObject* args);
