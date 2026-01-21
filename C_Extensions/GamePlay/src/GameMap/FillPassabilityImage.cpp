#include <GameMap/FillPassabilityImage.h>
#include <utils.h>
#include <stdexcept>

PyObject* C_FillPassabilityImage(PyObject* self, PyObject* args) {
    std::vector<PyObject*> tempPyObjects;
    try {
        PyObject *sizeObj;
        PyObject *imgObj;
        PyObject *lightMap;
        if (!PyArg_ParseTuple(args, "OOO", &sizeObj, &imgObj, &lightMap)) {
            return nullptr;
        }
    
        long width = GetAttrLong(sizeObj, "x", -1);
        long height = GetAttrLong(sizeObj, "y", -1);
        if (width <= 0 || height <= 0) {
            throw std::runtime_error("Invalid size object for C_FillPassabilityImage");
        }
    
        PyObject *engineModule = PyImport_ImportModule("Engine");
        CHECK_NULL(tempPyObjects, engineModule, "Failed to import Engine module");
        tempPyObjects.push_back(engineModule);
        PyObject *colorClass = PyObject_GetAttrString(engineModule, "Color");
        CHECK_NULL(tempPyObjects, colorClass, "Failed to get Color class from Engine module");
        tempPyObjects.push_back(colorClass);
        PyObject *vec2uClass = PyObject_GetAttrString(engineModule, "Vector2u");
        CHECK_NULL(tempPyObjects, vec2uClass, "Failed to get Vector2u class from Engine module");
        tempPyObjects.push_back(vec2uClass);
    
        for (long y = 0; y < height; ++y) {
            PyObject *row = PySequence_GetItem(lightMap, y);
            CHECK_NULL(tempPyObjects, row, "Invalid light map index");
            tempPyObjects.push_back(row);
            for (long x = 0; x < width; ++x) {
                PyObject *valObj = PySequence_GetItem(row, x);
                CHECK_NULL(tempPyObjects, valObj, "Invalid light map value");
                tempPyObjects.push_back(valObj);
                double v = PyFloat_AsDouble(valObj);
                int g = (int)(v * 255.0);
                if (g < 0) g = 0;
                if (g > 255) g = 255;
    
                PyObject *color = PyObject_CallFunction(colorClass, "iii", g, g, g);
                CHECK_NULL(tempPyObjects, color, "Failed to create Color object");
                tempPyObjects.push_back(color);
                PyObject *pos = PyObject_CallFunction(vec2uClass, "ii", (int)x, (int)y);
                CHECK_NULL(tempPyObjects, pos, "Failed to create Vector2u object");
                tempPyObjects.push_back(pos);
                DoMethod(imgObj, "setPixel", {pos, color});
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
