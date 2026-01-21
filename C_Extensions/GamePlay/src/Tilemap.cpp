#include <Tilemap.h>
#include <utils.h>
#include <vector>
#include <stdexcept>

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

PyObject* C_CalculateVertexArray(PyObject* self, PyObject* args) {
    std::vector<PyObject*> tempPyObjects;
    try {
        PyObject *vertexArray;
        PyObject *tiles;
        int tileSize, columns, width, height;
        int tu, tv, start;
    
        if (!PyArg_ParseTuple(args, "OOiiii", &vertexArray, &tiles, &tileSize, &columns, &width, &height)) {
            return nullptr;
        }
    
        for (int y = 0; y < height; ++y) {
            PyObject *row = PySequence_GetItem(tiles, y);
            CHECK_NULL(tempPyObjects, row, "Invalid tile index");
            tempPyObjects.push_back(row);
            for (int x = 0; x < width; ++x) {
                PyObject *item = PySequence_GetItem(row, x);
                CHECK_NULL(tempPyObjects, item, "Invalid tile index");
                tempPyObjects.push_back(item);
                int is_none = (item == Py_None);
                if (!is_none) {
                    int tileNumber = (int)PyLong_AsLong(item);
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
    
                    PyObject *v0 = GetVertex(vertexArray, start + 0);
                    CHECK_NULL(tempPyObjects, v0, "Invalid vertex index");
                    tempPyObjects.push_back(v0);
                    PyObject *v1 = GetVertex(vertexArray, start + 1);
                    CHECK_NULL(tempPyObjects, v1, "Invalid vertex index");
                    tempPyObjects.push_back(v1);
                    PyObject *v2 = GetVertex(vertexArray, start + 2);
                    CHECK_NULL(tempPyObjects, v2, "Invalid vertex index");
                    tempPyObjects.push_back(v2);
                    PyObject *v3 = GetVertex(vertexArray, start + 3);
                    CHECK_NULL(tempPyObjects, v3, "Invalid vertex index");
                    tempPyObjects.push_back(v3);
                    PyObject *v4 = GetVertex(vertexArray, start + 4);
                    CHECK_NULL(tempPyObjects, v4, "Invalid vertex index");
                    tempPyObjects.push_back(v4);
                    PyObject *v5 = GetVertex(vertexArray, start + 5);
                    CHECK_NULL(tempPyObjects, v5, "Invalid vertex index");
                    tempPyObjects.push_back(v5);
    
                    if (setVec2(v0, "position", x0, y0) != 0 || setVec2(v0, "texCoords", tx0, ty0) != 0 ||
                        setVec2(v1, "position", x1, y0) != 0 || setVec2(v1, "texCoords", tx1, ty0) != 0 ||
                        setVec2(v2, "position", x0, y1) != 0 || setVec2(v2, "texCoords", tx0, ty1) != 0 ||
                        setVec2(v3, "position", x0, y1) != 0 || setVec2(v3, "texCoords", tx0, ty1) != 0 ||
                        setVec2(v4, "position", x1, y0) != 0 || setVec2(v4, "texCoords", tx1, ty0) != 0 ||
                        setVec2(v5, "position", x1, y1) != 0 || setVec2(v5, "texCoords", tx1, ty1) != 0) {
                        ClearCache(tempPyObjects);
                        throw std::runtime_error("Failed to set vertex position or texture coordinates");
                    }
                }
            }
        }
        ClearCache(tempPyObjects);
        Py_RETURN_NONE;
    }
    catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        ClearCache(tempPyObjects);
        return nullptr;
    }
}
