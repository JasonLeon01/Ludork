#include <GameMap/FindPath.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <pybind11/stl.h>
#include <queue>


using Node = std::pair<int, IntPair>;

inline bool inBounds(int x, int y, int width, int height) {
  return x >= 0 && x < width && y >= 0 && y < height;
}

bool passable(int x, int y, int sx, int sy, int gx, int gy,
              const py::object &tilemap,
              const std::vector<std::string> &layerKeys,
              const ActorDict &actors,
              const std::unordered_map<std::string, TileGrids> &tileData,
              const py::function &getLayer, const py::function &getMapPosition,
              const py::function &getCollisionEnabled,
              const py::function &isPassable) {
  if ((x == sx && y == sy) || (x == gx && y == gy)) {
    return true;
  }
  for (auto &layerName : layerKeys) {
    auto layer = getLayer(tilemap, layerName);
    auto visible = layer.attr("visible").cast<bool>();
    if (!visible) {
      continue;
    }
    auto position = sf::Vector2i(x, y);
    auto it = actors.find(layerName);
    if (it != actors.end()) {
      auto layerActors = it->second;
      for (auto &actor : layerActors) {
        auto actorMapPosition = getMapPosition(actor).cast<sf::Vector2i>();
        if (actorMapPosition == position) {
          return !getCollisionEnabled(actor).cast<bool>();
        }
      }
    }
    auto tile = tileData.at(layerName)[position.y][position.x];
    if (tile.has_value()) {
      return isPassable(layer, position).cast<bool>();
    }
  }
  return true;
}

inline int getNodeFScore(const std::map<IntPair, int> &fScore,
                         const IntPair &node) {
  auto iter = fScore.find(node);
  if (iter == fScore.end()) {
    return 1 << 30;
  }
  return iter->second;
}

std::vector<sf::Vector2i>
C_FindPath(const sf::Vector2i &start, const sf::Vector2i &goal,
           const sf::Vector2u &size, const py::object &tilemap,
           const std::vector<std::string> &layerKeys, const ActorDict &actors,
           const std::unordered_map<std::string, TileGrids> &tileData,
           const py::function &getLayer, const py::function &getMapPosition,
           const py::function &getCollisionEnabled,
           const py::function &isPassable) {
  int sx = start.x;
  int sy = start.y;
  int gx = goal.x;
  int gy = goal.y;
  unsigned int width = size.x;
  unsigned int height = size.y;
  IntPair dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  IntPair start_t = {sx, sy};
  IntPair goal_t = {gx, gy};
  if (start_t == goal_t) {
    return {};
  }
  std::map<IntPair, IntPair> cameFrom;
  std::map<IntPair, int> gScore;
  gScore[start_t] = 0;
  std::map<IntPair, int> fScore;
  fScore[start_t] = std::abs(sx - gx) + std::abs(sy - gy);
  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openQueue;
  openQueue.push({fScore[start_t], start_t});
  while (!openQueue.empty()) {
    IntPair current = openQueue.top().second;
    int currentF = openQueue.top().first;
    openQueue.pop();
    if (currentF > getNodeFScore(fScore, current)) {
      continue;
    }
    if (current == goal_t) {
      std::vector<IntPair> pathPosition;
      auto c = current;
      while (cameFrom.find(c) != cameFrom.end()) {
        pathPosition.push_back(c);
        c = cameFrom[c];
      }
      std::reverse(pathPosition.begin(), pathPosition.end());
      std::vector<sf::Vector2i> moves;
      int px = sx;
      int py = sy;
      for (auto &[x, y] : pathPosition) {
        moves.emplace_back(x - px, y - py);
        px = x;
        py = y;
      }
      return moves;
    }
    auto [cx, cy] = current;
    for (auto &[dx, dy] : dirs) {
      int nx = cx + dx;
      int ny = cy + dy;
      if (!inBounds(nx, ny, width, height)) {
        continue;
      }
      if (!passable(nx, ny, sx, sy, gx, gy, tilemap, layerKeys, actors,
                    tileData, getLayer, getMapPosition, getCollisionEnabled,
                    isPassable)) {
        continue;
      }
      IntPair nt = {nx, ny};
      int tentative = gScore[current] + 1;
      int prevG = (gScore.count(nt)) ? gScore[nt] : (1 << 30);
      if (tentative < prevG) {
        cameFrom[nt] = current;
        gScore[nt] = tentative;
        int nextF = tentative + std::abs(nx - gx) + std::abs(ny - gy);
        fScore[nt] = nextF;
        openQueue.push({nextF, nt});
      }
    }
  }
  return {};
}

void ApplyFindPathBinding(py::module &m) {
  m.def("C_FindPath", &C_FindPath, py::arg("start"), py::arg("goal"),
        py::arg("size"), py::arg("tilemap"), py::arg("layerKeys"),
        py::arg("actors"), py::arg("getLayer"), py::arg("getMapPosition"),
        py::arg("getCollisionEnabled"), py::arg("getTile"),
        py::arg("isPassable"));
}
