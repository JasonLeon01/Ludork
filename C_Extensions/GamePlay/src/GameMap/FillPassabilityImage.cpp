#include <utils.h>
#include <GameMap/FillPassabilityImage.h>

PyObject* fillPassabilityImage(PyObject* self, PyObject* args) {
    PyObject *sizeObj;
    PyObject *imgObj;
    PyObject *lightMap;

    if (!PyArg_ParseTuple(args, "OOO", &sizeObj, &imgObj, &lightMap)) {
        return NULL;
    }

    long width = getAttrLong(sizeObj, "x", -1);
    long height = getAttrLong(sizeObj, "y", -1);
    if (width <= 0 || height <= 0) {
        PyErr_SetString(PyExc_ValueError, "Invalid size object for fillPassabilityImage");
        return NULL;
    }

    PyObject *engineModule = PyImport_ImportModule("Engine");
    if (!engineModule) {
        return NULL;
    }
    PyObject *colorClass = PyObject_GetAttrString(engineModule, "Color");
    PyObject *vec2uClass = PyObject_GetAttrString(engineModule, "Vector2u");
    Py_DECREF(engineModule);
    if (!colorClass || !vec2uClass) {
        Py_XDECREF(colorClass);
        Py_XDECREF(vec2uClass);
        return NULL;
    }

    for (long y = 0; y < height; ++y) {
        PyObject *row = PySequence_GetItem(lightMap, y);
        if (row == NULL) {
            Py_DECREF(colorClass);
            Py_DECREF(vec2uClass);
            return NULL;
        }

        for (long x = 0; x < width; ++x) {
            PyObject *valObj = PySequence_GetItem(row, x);
            if (valObj == NULL) {
                Py_DECREF(row);
                Py_DECREF(colorClass);
                Py_DECREF(vec2uClass);
                return NULL;
            }
            double v = PyFloat_AsDouble(valObj);
            Py_DECREF(valObj);
            if (PyErr_Occurred()) {
                Py_DECREF(row);
                Py_DECREF(colorClass);
                Py_DECREF(vec2uClass);
                return NULL;
            }

            int g = (int)(v * 255.0);
            if (g < 0) g = 0;
            if (g > 255) g = 255;

            PyObject *color = PyObject_CallFunction(colorClass, "iii", g, g, g);
            if (color == NULL) {
                Py_DECREF(row);
                Py_DECREF(colorClass);
                Py_DECREF(vec2uClass);
                return NULL;
            }

            PyObject *pos = PyObject_CallFunction(vec2uClass, "ii", (int)x, (int)y);
            if (pos == NULL) {
                Py_DECREF(color);
                Py_DECREF(row);
                Py_DECREF(colorClass);
                Py_DECREF(vec2uClass);
                return NULL;
            }

            PyObject *result = PyObject_CallMethod(imgObj, "setPixel", "OO", pos, color);
            Py_DECREF(color);
            Py_DECREF(pos);
            if (result == NULL) {
                Py_DECREF(row);
                Py_DECREF(colorClass);
                Py_DECREF(vec2uClass);
                return NULL;
            }
            Py_DECREF(result);
        }
        Py_DECREF(row);
    }

    Py_DECREF(colorClass);
    Py_DECREF(vec2uClass);
    Py_RETURN_NONE;
}
