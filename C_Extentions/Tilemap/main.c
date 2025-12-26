#include <Python.h>

#define coord_vec2_obj(objx, objy, x, y) PyObject *objx = PyFloat_FromDouble(x); PyObject *objy = PyFloat_FromDouble(y);
#define collect_vec(objx, objy, vec) Py_DECREF(objx); Py_DECREF(objy); Py_DECREF(vec);

static PyObject* getVertexItem(PyObject *vertexArray, Py_ssize_t index) {
    PyObject *vertex;
    if (PyList_Check(vertexArray)) {
        vertex = PyList_GET_ITEM(vertexArray, index);
    } else {
        PyObject *idx = PyLong_FromLong(index);
        if (!idx) return NULL;
        vertex = PyObject_GetItem(vertexArray, idx);
        Py_DECREF(idx);
    }
    if (!vertex) {
        PyErr_SetString(PyExc_IndexError, "Invalid vertex index");
        return NULL;
    }
    return vertex;
}

static int setVec2(PyObject *vertex, const char *attr, double x, double y) {
    PyObject *vec = PyObject_GetAttrString(vertex, attr);
    if (!vec) return -1;
    coord_vec2_obj(x_obj, y_obj, x, y);
    if (!x_obj || !y_obj) {
        collect_vec(x_obj, y_obj, vec);
        return -1;
    }
    int r1 = PyObject_SetAttrString(vec, "x", x_obj);
    int r2 = PyObject_SetAttrString(vec, "y", y_obj);
    collect_vec(x_obj, y_obj, vec);
    return (r1 == 0 && r2 == 0) ? 0 : -1;
}

static PyObject* calculateVertexArray(PyObject* self, PyObject* args) {
    PyObject *vertexArray;
    PyObject *tiles;
    int tileSize, columns, width, height;
    int tu, tv, start;

    if (!PyArg_ParseTuple(args, "OOiiii", &vertexArray, &tiles, &tileSize, &columns, &width, &height)) {
        return NULL;
    }

    for (int y = 0; y < height; ++y) {
        PyObject *row = PySequence_GetItem(tiles, y);
        if (!row) {
            PyErr_SetString(PyExc_IndexError, "Invalid tile index");
            return NULL;
        }
        for (int x = 0; x < width; ++x) {
            PyObject *item = PySequence_GetItem(row, x);
            if (!item) {
                Py_DECREF(row);
                PyErr_SetString(PyExc_IndexError, "Invalid tile index");
                return NULL;
            }
            int is_none = (item == Py_None);
            if (!is_none) {
                int tileNumber = (int)PyLong_AsLong(item);
                if (PyErr_Occurred()) {
                    Py_DECREF(item);
                    Py_DECREF(row);
                    PyErr_SetString(PyExc_TypeError, "Invalid tile number");
                    return NULL;
                }
                tu = tileNumber % columns;
                tv = tileNumber / columns;
                start = (x + y * width) * 6;

                double x0 = (double)(x * tileSize);
                double x1 = (double)((x + 1) * tileSize);
                double y0 = (double)(y * tileSize);
                double y1 = (double)((y + 1) * tileSize);
                double tx0 = (double)(tu * tileSize);
                double tx1 = (double)((tu + 1) * tileSize);
                double ty0 = (double)(tv * tileSize);
                double ty1 = (double)((tv + 1) * tileSize);

                PyObject *v0 = getVertexItem(vertexArray, start + 0);
                PyObject *v1 = getVertexItem(vertexArray, start + 1);
                PyObject *v2 = getVertexItem(vertexArray, start + 2);
                PyObject *v3 = getVertexItem(vertexArray, start + 3);
                PyObject *v4 = getVertexItem(vertexArray, start + 4);
                PyObject *v5 = getVertexItem(vertexArray, start + 5);
                if (!v0 || !v1 || !v2 || !v3 || !v4 || !v5) {
                    Py_XDECREF(v0); Py_XDECREF(v1); Py_XDECREF(v2);
                    Py_XDECREF(v3); Py_XDECREF(v4); Py_XDECREF(v5);
                    Py_DECREF(item);
                    Py_DECREF(row);
                    return NULL;
                }

                if (setVec2(v0, "position", x0, y0) != 0 || setVec2(v0, "texCoords", tx0, ty0) != 0 ||
                    setVec2(v1, "position", x1, y0) != 0 || setVec2(v1, "texCoords", tx1, ty0) != 0 ||
                    setVec2(v2, "position", x0, y1) != 0 || setVec2(v2, "texCoords", tx0, ty1) != 0 ||
                    setVec2(v3, "position", x0, y1) != 0 || setVec2(v3, "texCoords", tx0, ty1) != 0 ||
                    setVec2(v4, "position", x1, y0) != 0 || setVec2(v4, "texCoords", tx1, ty0) != 0 ||
                    setVec2(v5, "position", x1, y1) != 0 || setVec2(v5, "texCoords", tx1, ty1) != 0) {
                    Py_DECREF(v0); Py_DECREF(v1); Py_DECREF(v2);
                    Py_DECREF(v3); Py_DECREF(v4); Py_DECREF(v5);
                    Py_DECREF(item);
                    Py_DECREF(row);
                    return NULL;
                }
                Py_DECREF(v0); Py_DECREF(v1); Py_DECREF(v2);
                Py_DECREF(v3); Py_DECREF(v4); Py_DECREF(v5);
            }
            Py_DECREF(item);
        }
        Py_DECREF(row);
    }
    Py_RETURN_NONE;
}

static PyMethodDef TilemapExtensionMethods[] = {
    {"calculateVertexArray", calculateVertexArray, METH_VARARGS},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef TilemapExtensionModule = {
    PyModuleDef_HEAD_INIT,
    "TilemapExtension",
    NULL,
    -1,
    TilemapExtensionMethods
};

PyMODINIT_FUNC PyInit_TilemapExtension(void) {
    return PyModule_Create(&TilemapExtensionModule);
}
