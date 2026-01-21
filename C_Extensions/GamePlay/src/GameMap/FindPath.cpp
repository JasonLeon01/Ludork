#include <GameMap/FindPath.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <utils.h>
#include <vector>

inline bool inBounds(int x, int y, int width, int height) {
  return x >= 0 && x < width && y >= 0 && y < height;
}

int passable(int x, int y, int sx, int sy, int gx, int gy, PyObject *tilemap,
             PyObject *layerKeys, PyObject *actors, PyObject *Vector2iClass) {
  std::vector<PyObject *> tempPyObjects;
  try {
    if ((x == sx && y == sy) || (x == gx && y == gy)) {
      return 1;
    }
    int len = PyList_Size(layerKeys);
    for (int i = 0; i < len; ++i) {
      PyObject *layerName = PyList_GetItem(layerKeys, i);
      PyObject *layer = GetMethodResult(tilemap, "getLayer", {layerName});
      CHECK_NULL(tempPyObjects, layer, "Failed to get layer from tilemap");
      tempPyObjects.push_back(layer);
      bool visible = GetAttrBool(layer, "visible", false);
      if (!visible) {
        continue;
      }
      PyObject *position = PyObject_CallFunction(Vector2iClass, "ii", x, y);
      if (!position) {
        PyErr_Clear();
        position = PyObject_CallFunction(Vector2iClass, "ii", x, y);
      }
      CHECK_NULL(tempPyObjects, position, "Failed to create position");
      tempPyObjects.push_back(position);
      if (PyDict_Contains(actors, layerName)) {
        PyObject *layerActorsObj = PyDict_GetItem(actors, layerName);
        auto layerActors = GetPyListItems(layerActorsObj);
        if (!layerActors.has_value()) {
          ClearCache(tempPyObjects);
          throw std::runtime_error(
              "Failed to get layerActors from layerActorsObj");
        }
        for (auto &actor : layerActors.value()) {
          PyObject *actorMapPosition =
              GetMethodResult(actor, "getMapPosition", {});
          CHECK_NULL(tempPyObjects, actorMapPosition,
                     "Failed to get actorMapPosition from actor");
          tempPyObjects.push_back(actorMapPosition);
          int eq = PyObject_RichCompareBool(actorMapPosition, position, Py_EQ);
          if (eq == -1) {
            ClearCache(tempPyObjects);
            throw std::runtime_error(
                "Failed to compare actorMapPosition with position");
          }
          if (eq) {
            bool result = !IsMethodTrue(actor, "getCollisionEnabled", {});
            if (PyErr_Occurred()) {
              ClearCache(tempPyObjects);
              throw std::runtime_error(
                  "Failed to getCollisionEnabled from actor");
            }
            ClearCache(tempPyObjects);
            return result ? 1 : 0;
          }
        }
      }
      PyObject *tile = GetMethodResult(layer, "get", {position});
      CHECK_NULL(tempPyObjects, tile, "Failed to get tile from layer");
      tempPyObjects.push_back(tile);
      if (tile != Py_None) {
        bool passable = IsMethodTrue(layer, "isPassable", {position});
        if (PyErr_Occurred()) {
          ClearCache(tempPyObjects);
          throw std::runtime_error("Failed to call isPassable on layer");
        }
        ClearCache(tempPyObjects);
        return passable ? 1 : 0;
      }
    }
    ClearCache(tempPyObjects);
    return 1;
  } catch (const std::exception &e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    ClearCache(tempPyObjects);
    return -1;
  }
}

inline int getNodeFScore(const std::map<IntPair, int> &fScore,
                         const IntPair &node) {
  auto iter = fScore.find(node);
  if (iter == fScore.end()) {
    return 1 << 30;
  }
  return iter->second;
}

PyObject *C_FindPath(PyObject *self, PyObject *args) {
  std::vector<PyObject *> tempPyObjects;
  try {
    PyObject *start;
    PyObject *goal;
    PyObject *size;
    PyObject *tilemap;
    PyObject *layerKeys;
    PyObject *actors;
    if (!PyArg_ParseTuple(args, "OOOOOO", &start, &goal, &size, &tilemap,
                          &layerKeys, &actors)) {
      return nullptr;
    }
    int sx = GetAttrLong(start, "x", 0);
    int sy = GetAttrLong(start, "y", 0);
    int gx = GetAttrLong(goal, "x", 0);
    int gy = GetAttrLong(goal, "y", 0);
    int width = GetAttrLong(size, "x", 0);
    int height = GetAttrLong(size, "y", 0);
    IntPair dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    IntPair start_t = {sx, sy};
    IntPair goal_t = {gx, gy};
    if (start_t == goal_t) {
      return PyList_New(0);
    }
    std::set<IntPair> openSet;
    openSet.insert(start_t);
    if (openSet.empty()) {
      ClearCache(tempPyObjects);
      throw std::runtime_error("Open set is empty");
    }
    PyObject *engineModule = PyImport_ImportModule("Engine");
    CHECK_NULL(tempPyObjects, engineModule, "Failed to import Engine module");
    tempPyObjects.push_back(engineModule);
    PyObject *Vector2iClass = PyObject_GetAttrString(engineModule, "Vector2i");
    CHECK_NULL(tempPyObjects, Vector2iClass,
               "Failed to get Vector2i class from Engine module");
    tempPyObjects.push_back(Vector2iClass);
    std::map<IntPair, IntPair> cameFrom;
    std::map<IntPair, int> gScore;
    gScore[start_t] = 0;
    std::map<IntPair, int> fScore;
    fScore[start_t] = std::abs(sx - gx) + std::abs(sy - gy);
    while (!openSet.empty()) {
      auto minNodeIter = min_element(
          openSet.begin(), openSet.end(),
          [&fScore](const IntPair &a, const IntPair &b) {
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
        for (auto &[x, y] : pathPosition) {
          PyObject *xObj = PyLong_FromLong(x - px);
          CHECK_NULL(tempPyObjects, xObj, "Failed to create xObj");
          tempPyObjects.push_back(xObj);
          PyObject *yObj = PyLong_FromLong(y - py);
          CHECK_NULL(tempPyObjects, yObj, "Failed to create yObj");
          tempPyObjects.push_back(yObj);
          PyObject *thisMove = NewInstance(Vector2iClass, {xObj, yObj});
          CHECK_NULL(tempPyObjects, thisMove, "Failed to create Vector2i move");
          tempPyObjects.push_back(thisMove);
          DoMethod(moves, "append", {thisMove});
          px = x;
          py = y;
        }
        ClearCache(tempPyObjects);
        return moves;
      }
      openSet.erase(minNodeIter);
      auto [cx, cy] = current;
      for (auto &[dx, dy] : dirs) {
        int nx = cx + dx;
        int ny = cy + dy;
        if (!inBounds(nx, ny, width, height)) {
          continue;
        }
        int passableResult = passable(nx, ny, sx, sy, gx, gy, tilemap,
                                      layerKeys, actors, Vector2iClass);
        if (passableResult < 0) {
          ClearCache(tempPyObjects);
          return nullptr;
        }
        if (passableResult == 0) {
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
    ClearCache(tempPyObjects);
    return PyList_New(0);
  } catch (const std::exception &e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    ClearCache(tempPyObjects);
    return nullptr;
  }
}
