#include <utils.h>
#include <GameMap/CExtensionFindPath.h>
#include <cmath>
#include <algorithm>
#include <set>
#include <map>
#include <vector>

inline bool inBounds(int x, int y, int width, int height) {
    return x >= 0 && x < width && y >= 0 && y < height;
}

inline bool passable(int x, int y, int sx, int sy, int gx, int gy, PyObject *tilemap, PyObject *layerKeys, PyObject *actors, PyObject *Vector2iClass) {
    if ((x == sx && y == sy) || (x == gx && y == gy)) {
        return true;
    }
    if (layerKeys == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get keys from layers dictionary");
        return false;
    }
    int len = PyList_Size(layerKeys);
    for (int i = 0; i < len; ++i) {
        PyObject *layerName = PyList_GetItem(layerKeys, i);
        PyObject *layer = PyObject_CallMethod(tilemap, "getLayer", "O", layerName);
        if (layer == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to get layer from tilemap");
            return false;
        }
        bool visible = getAttrBool(layer, "visible", false);
        if (!visible) {
            Py_DECREF(layer);
            continue;
        }
        PyObject *position = PyObject_CallFunction(Vector2iClass, "ii", x, y);
        if (position == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create Vector2i position");
            Py_DECREF(layer);
            return false;
        }
        if (PyDict_Contains(actors, layerName)) {
            PyObject *layerActors = PyDict_GetItem(actors, layerName);
            int layerActorsLen = PyList_Size(layerActors);
            for (int i = 0; i < layerActorsLen; ++i) {
                PyObject *actor = PyList_GetItem(layerActors, i);
                if (actor == NULL) {
                    PyErr_SetString(PyExc_RuntimeError, "Failed to get actor from layerActors");
                    Py_DECREF(layer);
                    Py_DECREF(position);
                    return false;
                }
                PyObject *actorMapPosition = PyObject_CallMethod(actor, "getMapPosition", NULL);
                if (actorMapPosition == NULL) {
                    PyErr_SetString(PyExc_RuntimeError, "Failed to get actorMapPosition from actor");
                    Py_DECREF(layer);
                    Py_DECREF(position);
                    return false;
                }
                int equal = PyObject_RichCompareBool(actorMapPosition, position, Py_EQ);
                Py_DECREF(actorMapPosition);
                if (equal < 0) {
                    Py_DECREF(layer);
                    Py_DECREF(position);
                    return false;
                }
                if (equal > 0) {
                    bool result = !isMethodTrue(actor, "getCollisionEnabled");
                    Py_DECREF(layer);
                    Py_DECREF(position);
                    return result;
                }
            }
        }
        PyObject *tile = PyObject_CallMethod(layer, "get", "O", position);
        if (tile == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to get tile from layer");
            Py_DECREF(layer);
            Py_DECREF(position);
            return false;
        }
        if (tile != Py_None) {
            PyObject *passableObj = PyObject_CallMethod(layer, "isPassable", "O", position);
            if (passableObj == NULL) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to call isPassable method");
                Py_DECREF(layer);
                Py_DECREF(position);
                Py_DECREF(tile);
                return false;
            }
            bool result = PyObject_IsTrue(passableObj);
            Py_DECREF(layer);
            Py_DECREF(position);
            Py_DECREF(tile);
            Py_DECREF(passableObj);
            return result;
        }
        Py_DECREF(layer);
        Py_DECREF(position);
        Py_DECREF(tile);
    }
    return true;
}

inline int getNodeFScore(const std::map<IntPair, int>& fScore, const IntPair& node) {
    auto iter = fScore.find(node);
    if (iter == fScore.end()) {
        return 1 << 30;
    }
    return iter->second;
}

PyObject* CExtensionFindPath(PyObject* self, PyObject* args) {
    PyObject *start;
    PyObject *goal;
    PyObject *size;
    PyObject *tilemap;
    PyObject *layerKeys;
    PyObject *actors;
    if (!PyArg_ParseTuple(args, "OOOOOO", &start, &goal, &size, &tilemap, &layerKeys, &actors)) {
        return NULL;
    }
    int sx = getAttrLong(start, "x", 0);
    int sy = getAttrLong(start, "y", 0);
    int gx = getAttrLong(goal, "x", 0);
    int gy = getAttrLong(goal, "y", 0);
    int width = getAttrLong(size, "x", 0);
    int height = getAttrLong(size, "y", 0);
    IntPair dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    IntPair start_t = {sx, sy};
    IntPair goal_t = {gx, gy};
    if (start_t == goal_t) {
        return PyList_New(0);
    }
    std::set<IntPair> openSet;
    openSet.insert(start_t);
    if (openSet.empty()) {
        return NULL;
    }
    PyObject *engineModule = PyImport_ImportModule("Engine");
    if (!engineModule) {
        return NULL;
    }
    PyObject *Vector2iClass = PyObject_GetAttrString(engineModule, "Vector2i");
    if (!Vector2iClass) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get Vector2i class from Engine module");
        Py_DECREF(engineModule);
        return NULL;
    }
    Py_DECREF(engineModule);
    std::map<IntPair, IntPair> cameFrom;
    std::map<IntPair, int> gScore;
    gScore[start_t] = 0;
    std::map<IntPair, int> fScore;
    fScore[start_t] = std::abs(sx - gx) + std::abs(sy - gy);
    while (!openSet.empty()) {
        auto minNodeIter = min_element(openSet.begin(), openSet.end(), [&fScore](const IntPair& a, const IntPair& b) {
            return getNodeFScore(fScore, a) < getNodeFScore(fScore, b);
        });
        IntPair current = *minNodeIter;
        if (current == goal_t) {
            std::vector<IntPair> pathPosition;
            auto c = current;
            while (cameFrom.find(c) != cameFrom.end()) {
                pathPosition.push_back(c);
                c = cameFrom[c];
            }
            std::reverse(pathPosition.begin(), pathPosition.end());
            PyObject *moves = PyList_New(0);
            int px = sx;
            int py = sy;
            for (auto& [x, y] : pathPosition) {
                PyObject *thisMove = PyObject_CallFunction(Vector2iClass, "ii", x - px, y - py);
                if (thisMove == NULL) {
                    PyErr_SetString(PyExc_RuntimeError, "Failed to create Vector2i move");
                    Py_DECREF(Vector2iClass);
                    Py_DECREF(moves);
                    return NULL;
                }
                PyObject_CallMethod(moves, "append", "O", thisMove);
                px = x;
                py = y;
                Py_DECREF(thisMove);
            }
            Py_DECREF(Vector2iClass);
            return moves;
        }
        openSet.erase(minNodeIter);
        auto [cx, cy] = current;
        for (auto& [dx, dy] : dirs) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (!inBounds(nx, ny, width, height)) {
                continue;
            }
            if (!passable(nx, ny, sx, sy, gx, gy, tilemap, layerKeys, actors, Vector2iClass)) {
                continue;
            }
            IntPair nt = {nx, ny};
            int tentative = gScore[current] + 1;
            int ntInGScore = 1 << 30;
            if (gScore.find(nt) != gScore.end()) {
                ntInGScore = gScore[nt];
            }
            if (tentative < ntInGScore) {
                cameFrom[nt] = current;
                gScore[nt] = tentative;
                fScore[nt] = tentative + std::abs(nx - gx) + std::abs(ny - gy);
                openSet.insert(nt);
            }
        }
    }
    Py_DECREF(Vector2iClass);
    return PyList_New(0);
}
