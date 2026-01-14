#include <utils.h>

long getAttrLong(PyObject* obj, const char* attr_name, long default_val) {
    if (!obj) return default_val;
    PyObject* attr = PyObject_GetAttrString(obj, attr_name);
    if (!attr) {
        PyErr_Clear();
        return default_val;
    }
    long val = PyLong_AsLong(attr);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        val = default_val;
    }
    Py_DECREF(attr);
    return val;
}

bool getAttrBool(PyObject* obj, const char* attr_name, bool default_val) {
    if (!obj) return default_val;
    PyObject* attr = PyObject_GetAttrString(obj, attr_name);
    if (!attr) {
        PyErr_Clear();
        return default_val;
    }
    bool val = PyObject_IsTrue(attr);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        val = default_val;
    }
    Py_DECREF(attr);
    return val;
}

bool isMethodTrue(PyObject* obj, const char* method_name) {
    if (!obj) return false;
    PyObject* method = PyObject_GetAttrString(obj, method_name);
    if (!method) {
        PyErr_Clear();
        return false;
    }
    bool val = PyObject_IsTrue(method);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        val = false;
    }
    Py_DECREF(method);
    return val;
}
