#include "GameMapExt.hpp"

#include <Gameplay/ActorCore.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>

using Node = std::pair<int, IntPair>;

inline bool inBounds(int x, int y, int width, int height) { return x >= 0 && x < width && y >= 0 && y < height; }

std::size_t IntPairHash::operator()(const IntPair &value) const {
    std::size_t xHash = std::hash<int>{}(value.first);
    std::size_t yHash = std::hash<int>{}(value.second);
    return xHash ^ (yHash + 0x9e3779b9 + (xHash << 6) + (xHash >> 2));
}

inline int getNodeFScore(const std::map<IntPair, int> &fScore, const IntPair &node) {
    auto iter = fScore.find(node);
    if (iter == fScore.end()) {
        return 1 << 30;
    }
    return iter->second;
}

GameMapExt::GameMapExt(sf::Shader *shader) { materialShader_ = shader; }

void GameMapExt::refreshShader(const sf::RenderTexture &lightMask, float screenScale, const sf::Vector2f &screenSize,
                               const sf::Vector2f &viewPos, float viewRot, const sf::Vector2f &gridSize, int cellSize,
                               const std::vector<Light> &lights, const sf::Color &ambientColor) {
    if (!materialShader_) {
        return;
    }
    auto shader = materialShader_;
    shader->setUniform("lightMask", lightMask.getTexture());
    shader->setUniform("screenScale", screenScale);
    shader->setUniform("screenSize", screenSize);
    shader->setUniform("viewPos", viewPos);
    shader->setUniform("viewRot", viewRot);
    shader->setUniform("gridSize", gridSize);
    shader->setUniform("cellSize", cellSize);
    shader->setUniform("lightLen", int(lights.size()));
    for (int i = 0; i < lights.size(); ++i) {
        const auto &light = lights[i];
        shader->setUniform(getUniformArrayName("lightPos", i), light.position);
        shader->setUniform(getUniformArrayName("lightColor", i), castFromColor(light.colour));
        shader->setUniform(getUniformArrayName("lightRadius", i), light.radius);
        shader->setUniform(getUniformArrayName("lightIntensity", i), light.intensity);
    }
    shader->setUniform("ambientColor", castAmbientFromColor(ambientColor));
}

sf::Texture *GameMapExt::generateDataFromMap(const sf::Vector2u &size,
                                             const std::vector<std::vector<float>> &materialMap, bool smooth) {
    int dataLen = size.x * size.y * 4;
    std::vector<std::uint8_t> pixelData(dataLen);
    for (int y = 0; y < size.y; ++y) {
        for (int x = 0; x < size.x; ++x) {
            int index = (y * size.x + x) * 4;
            pixelData[index] = std::uint8_t(materialMap[y][x] * 255.0f);
            pixelData[index + 1] = pixelData[index];
            pixelData[index + 2] = pixelData[index];
            pixelData[index + 3] = 255;
        }
    }

    sf::Image img(size, pixelData.data());
    sf::Texture *texture = new sf::Texture();
    if (!texture->loadFromImage(img)) {
        throw std::runtime_error("Failed to load texture from image at method generateDataFromMap");
    }
    texture->setSmooth(smooth);
    return texture;
}

PathResult GameMapExt::findPathExt(const sf::Vector2i &start, const sf::Vector2i &goal, const sf::Vector2u &size,
                                   ActorCore &movingActor) {
    int sx = start.x;
    int sy = start.y;
    int gx = goal.x;
    int gy = goal.y;
    unsigned int width = size.x;
    unsigned int height = size.y;
    IntPair dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    IntPair start_t = {sx, sy};
    IntPair goal_t = {gx, gy};
    PathResult result;
    if (start_t == goal_t) {
        result.route.emplace_back(sx, sy);
        return result;
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
            result.offsets.reserve(pathPosition.size());
            result.points.reserve(pathPosition.size());
            result.route.reserve(pathPosition.size() + 1);
            result.route.emplace_back(sx, sy);
            int px = sx;
            int py = sy;
            for (auto &[x, y] : pathPosition) {
                result.offsets.emplace_back(x - px, y - py);
                result.points.emplace_back(x, y);
                result.route.emplace_back(x, y);
                px = x;
                py = y;
            }
            return result;
        }
        auto [cx, cy] = current;
        for (auto &[dx, dy] : dirs) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (!inBounds(nx, ny, width, height)) {
                continue;
            }
            if (!nodePassableForActor(nx, ny, sx, sy, gx, gy, movingActor)) {
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
    return result;
}

std::vector<std::vector<py::object>> GameMapExt::getMaterialPropertyMapExt(int width, int height,
                                                                           const std::string &functionName,
                                                                           const py::object &invalidValue) {
    std::vector<std::vector<py::object>> materialPropertyMap;
    for (int y = 0; y < height; ++y) {
        materialPropertyMap.push_back({});
        for (int x = 0; x < width; ++x) {
            materialPropertyMap[y].push_back(getMaterialProperty({x, height - y - 1}, functionName, invalidValue));
        }
    }
    return materialPropertyMap;
}

void GameMapExt::syncActorsRef(const ActorDict &actors) {
    actorsCoreRef_.clear();
    for (const auto &[layerName, actorList] : actors) {
        auto &cores = actorsCoreRef_[layerName];
        cores.reserve(actorList.size());
        for (const py::object &actor : actorList) {
            cores.push_back(&actor.cast<ActorCore &>());
        }
    }
}

std::vector<std::vector<bool>> GameMapExt::rebuildPassabilityCache(const sf::Vector2u &size) {
    unsigned int width = size.x;
    unsigned int height = size.y;
    std::vector<std::vector<bool>> tilePassableGrid(height);
    occupancyMap_.clear();
    for (unsigned int y = 0; y < height; ++y) {
        std::vector<bool> row(width);
        for (unsigned int x = 0; x < width; ++x) {
            bool passable = true;
            for (auto &layerName : layerKeysRef) {
                if (!layerVisible(layerName)) {
                    continue;
                }
                if (tryGetLayerPassability(layerName, static_cast<int>(x), static_cast<int>(y), passable)) {
                    break;
                }
            }
            row[x] = passable;
        }
        tilePassableGrid[y] = row;
    }
    for (auto &[_, coreList] : actorsCoreRef_) {
        for (ActorCore *core : coreList) {
            registerActorOccupancy(*core);
        }
    }
    return tilePassableGrid;
}

std::vector<py::object> GameMapExt::getActorsAt(int x, int y) {
    auto it = occupancyMap_.find({x, y});
    if (it == occupancyMap_.end()) {
        return {};
    }
    std::vector<py::object> result;
    result.reserve(it->second.size());
    for (ActorCore *core : it->second) {
        result.push_back(core->getPythonActor());
    }
    return result;
}

std::vector<py::object> GameMapExt::getActorsInRange(int x, int y, int radius) {
    std::vector<py::object> result;
    for (int ix = x - radius; ix <= x + radius; ++ix) {
        for (int iy = y - radius; iy <= y + radius; ++iy) {
            auto it = occupancyMap_.find({ix, iy});
            if (it == occupancyMap_.end()) {
                continue;
            }
            for (ActorCore *core : it->second) {
                result.push_back(core->getPythonActor());
            }
        }
    }
    return result;
}

std::vector<py::object> GameMapExt::getCollisionAt(int x, int y, ActorCore &selfCore) {
    if (!selfCore.getCollisionEnabled()) {
        return {};
    }
    std::vector<py::object> result;
    auto it = occupancyMap_.find({x, y});
    if (it == occupancyMap_.end()) {
        return result;
    }
    const auto &descendantActors = selfCore.getDescendantIds();
    for (ActorCore *otherCore : it->second) {
        if (otherCore == &selfCore) {
            continue;
        }
        if (descendantActors.find(otherCore) != descendantActors.end()) {
            continue;
        }
        if (otherCore->isDestroyed()) {
            continue;
        }
        if (otherCore->getCollisionEnabled()) {
            result.push_back(otherCore->getPythonActor());
        }
    }
    return result;
}

std::vector<py::object> GameMapExt::getOverlapsAt(int x, int y, ActorCore &selfCore) {
    std::vector<py::object> result;
    auto it = occupancyMap_.find({x, y});
    if (it == occupancyMap_.end()) {
        return result;
    }
    const auto &descendantActors = selfCore.getDescendantIds();
    for (ActorCore *otherCore : it->second) {
        if (otherCore == &selfCore) {
            continue;
        }
        if (descendantActors.find(otherCore) != descendantActors.end()) {
            continue;
        }
        if (otherCore->isDestroyed()) {
            continue;
        }
        result.push_back(otherCore->getPythonActor());
    }
    return result;
}

sf::Vector3f GameMapExt::castFromColor(const sf::Color &color) {
    return sf::Vector3f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
}

sf::Vector3f GameMapExt::castAmbientFromColor(const sf::Color &color) {
    const float alpha = color.a / 255.0f;
    return sf::Vector3f(color.r / 255.0f * alpha, color.g / 255.0f * alpha, color.b / 255.0f * alpha);
}

std::string GameMapExt::getUniformArrayName(const std::string &name, int count) {
    auto it = uniformArrayNameCache_.find({name, count});
    if (it != uniformArrayNameCache_.end()) {
        return it->second;
    }
    std::string arrayName = name + "[" + std::to_string(count) + "]";
    uniformArrayNameCache_.insert({{name, count}, arrayName});
    return arrayName;
}

bool GameMapExt::passable(int x, int y, int sx, int sy, int gx, int gy) {
    if ((x == sx && y == sy) || (x == gx && y == gy)) {
        return true;
    }
    auto occupancyIt = occupancyMap_.find({x, y});
    if (occupancyIt != occupancyMap_.end()) {
        for (ActorCore *core : occupancyIt->second) {
            if (core->isDestroyed()) {
                continue;
            }
            if (core->blocksPassability()) {
                return false;
            }
        }
    }
    for (auto &layerName : layerKeysRef) {
        if (!layerVisible(layerName)) {
            continue;
        }
        bool layerPassable = true;
        if (tryGetLayerPassability(layerName, x, y, layerPassable)) {
            return layerPassable;
        }
    }
    return true;
}

void GameMapExt::registerActorOccupancy(ActorCore &core) {
    for (const sf::Vector2i &cell : core.getOccupiedMapCells()) {
        auto key = std::make_pair(cell.x, cell.y);
        auto &actorsAtCell = occupancyMap_[key];
        if (std::find(actorsAtCell.begin(), actorsAtCell.end(), &core) == actorsAtCell.end()) {
            actorsAtCell.push_back(&core);
        }
    }
}

bool GameMapExt::nodePassableForActor(int x, int y, int sx, int sy, int gx, int gy, const ActorCore &movingCore) {
    const std::vector<sf::Vector2i> cells = movingCore.getOccupiedMapCellsAtMapPosition({x, y});
    if (cells.empty()) {
        return passable(x, y, sx, sy, gx, gy);
    }
    for (const sf::Vector2i &cell : cells) {
        if (!passable(cell.x, cell.y, sx, sy, gx, gy)) {
            return false;
        }
    }
    return true;
}

bool GameMapExt::layerVisible(const std::string &layerName) const {
    auto it = layerVisibleRef.find(layerName);
    if (it == layerVisibleRef.end()) {
        return true;
    }
    return it->second;
}

bool GameMapExt::tryGetLayerPassability(const std::string &layerName, int x, int y, bool &outPassable) const {
    if (x < 0 || y < 0) {
        return false;
    }
    std::size_t ux = static_cast<std::size_t>(x);
    std::size_t uy = static_cast<std::size_t>(y);

    auto autoGridIt = autoTileDataRef.find(layerName);
    if (autoGridIt != autoTileDataRef.end()) {
        const auto &autoGrid = autoGridIt->second;
        if (uy < autoGrid.size() && ux < autoGrid[uy].size() && autoGrid[uy][ux].has_value()) {
            int autoTileIndex = autoGrid[uy][ux].value();
            auto passableIt = autoTilePassableRef.find(layerName);
            if (passableIt == autoTilePassableRef.end()) {
                outPassable = false;
                return true;
            }
            const auto &passableList = passableIt->second;
            if (autoTileIndex < 0 || autoTileIndex >= static_cast<int>(passableList.size())) {
                outPassable = false;
                return true;
            }
            outPassable = passableList[autoTileIndex];
            return true;
        }
    }

    auto tileGridIt = tileDataRef.find(layerName);
    if (tileGridIt == tileDataRef.end()) {
        return false;
    }
    const auto &tileGrid = tileGridIt->second;
    if (uy >= tileGrid.size() || ux >= tileGrid[uy].size() || !tileGrid[uy][ux].has_value()) {
        return false;
    }

    int tileIndex = tileGrid[uy][ux].value();
    auto passableIt = tilePassableRef.find(layerName);
    if (passableIt == tilePassableRef.end()) {
        outPassable = false;
        return true;
    }
    const auto &passableList = passableIt->second;
    if (tileIndex < 0 || tileIndex >= static_cast<int>(passableList.size())) {
        outPassable = false;
        return true;
    }
    outPassable = passableList[tileIndex];
    return true;
}

py::object GameMapExt::getMaterialProperty(const sf::Vector2i pos, const std::string &functionName,
                                           const py::object &invalidValue) {
    for (auto &layerName : layerKeysRef) {
        auto layer = getLayer(tilemapRef, layerName);
        if (layer.is_none()) {
            continue;
        }
        if (!layer.attr("visible").cast<bool>()) {
            continue;
        }
        auto it = actorsCoreRef_.find(layerName);
        if (it != actorsCoreRef_.end()) {
            for (ActorCore *core : it->second) {
                if (core->getMapPosition() == pos) {
                    py::object actor = core->getPythonActor();
                    auto value = actor.attr(functionName.c_str())();
                    if (!value.equal(invalidValue)) {
                        return value;
                    }
                }
            }
        }
        auto tileObj = tileDataRef.at(layerName)[pos.y][pos.x];
        bool hasContent = tileObj.has_value();
        if (!hasContent) {
            auto autoIt = autoTileDataRef.find(layerName);
            if (autoIt != autoTileDataRef.end()) {
                auto &autoGrid = autoIt->second;
                if (static_cast<std::size_t>(pos.y) < autoGrid.size() &&
                    static_cast<std::size_t>(pos.x) < autoGrid[pos.y].size() &&
                    autoGrid[pos.y][pos.x].has_value()) {
                    hasContent = true;
                }
            }
        }
        if (hasContent) {
            auto value = layer.attr(functionName.c_str())(pos);
            return value;
        }
    }
    return invalidValue;
}
