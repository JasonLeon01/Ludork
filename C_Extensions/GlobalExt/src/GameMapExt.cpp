#include "GameMapExt.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>

using Node = std::pair<int, IntPair>;

inline bool inBounds(int x, int y, int width, int height) { return x >= 0 && x < width && y >= 0 && y < height; }

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
                               const std::vector<py::object> &lights, const sf::Color &ambientColor) {
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
        auto light = lights[i];
        auto c = light.attr("colour").cast<sf::Color>();
        shader->setUniform(getUniformArrayName("lightPos", i), light.attr("position").cast<sf::Vector2f>());
        shader->setUniform(getUniformArrayName("lightColor", i), castFromColor(c));
        shader->setUniform(getUniformArrayName("lightRadius", i), light.attr("radius").cast<float>());
        shader->setUniform(getUniformArrayName("lightIntensity", i), light.attr("intensity").cast<float>());
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

std::vector<sf::Vector2i> GameMapExt::findPathExt(const sf::Vector2i &start, const sf::Vector2i &goal,
                                                  const sf::Vector2u &size) {
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
            if (!passable(nx, ny, sx, sy, gx, gy)) {
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

std::pair<std::vector<std::vector<bool>>, std::map<std::pair<int, int>, std::vector<py::object>>>
GameMapExt::rebuildPassabilityCache(const sf::Vector2u &size) {
    unsigned int width = size.x;
    unsigned int height = size.y;
    std::vector<std::vector<bool>> tilePassableGrid(height);
    std::map<std::pair<int, int>, std::vector<py::object>> occupancyMap;
    for (unsigned int y = 0; y < height; ++y) {
        std::vector<bool> row(width);
        for (unsigned int x = 0; x < width; ++x) {
            bool passable = true;
            for (auto &layerName : layerKeysRef) {
                auto tile = tileDataRef.at(layerName).at(y).at(x);
                bool hasContent = tile.has_value();
                if (!hasContent) {
                    auto autoIt = autoTileDataRef.find(layerName);
                    if (autoIt != autoTileDataRef.end()) {
                        auto &autoGrid = autoIt->second;
                        if (y < autoGrid.size() && x < autoGrid[y].size() && autoGrid[y][x].has_value()) {
                            hasContent = true;
                        }
                    }
                }
                if (hasContent) {
                    auto layer = getLayer(tilemapRef, layerName);
                    passable = TileLayerPassable(layer, sf::Vector2i(x, y)).cast<bool>();
                    break;
                }
            }
            row[x] = passable;
        }
        tilePassableGrid[y] = row;
    }
    for (auto &[_, actorList] : actorsRef) {
        for (auto &other : actorList) {
            if (!getCollisionEnabled(other).cast<bool>()) {
                continue;
            }
            auto pos = getMapPosition(other).cast<sf::Vector2i>();
            auto key = std::make_pair(pos.x, pos.y);
            if (occupancyMap.find(key) == occupancyMap.end()) {
                occupancyMap[key] = std::vector<py::object>();
            }
            occupancyMap[key].push_back(other);
        }
    }
    return {tilePassableGrid, occupancyMap};
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
    for (auto &layerName : layerKeysRef) {
        auto layer = getLayer(tilemapRef, layerName);
        auto visible = layer.attr("visible").cast<bool>();
        if (!visible) {
            continue;
        }
        auto position = sf::Vector2i(x, y);
        auto it = actorsRef.find(layerName);
        if (it != actorsRef.end()) {
            auto layerActors = it->second;
            for (auto &actor : layerActors) {
                auto actorMapPosition = getMapPosition(actor).cast<sf::Vector2i>();
                if (actorMapPosition == position) {
                    return !getCollisionEnabled(actor).cast<bool>();
                }
            }
        }
        auto tile = tileDataRef.at(layerName)[position.y][position.x];
        bool hasContent = tile.has_value();
        if (!hasContent) {
            auto autoIt = autoTileDataRef.find(layerName);
            if (autoIt != autoTileDataRef.end()) {
                auto &autoGrid = autoIt->second;
                if (static_cast<std::size_t>(position.y) < autoGrid.size() &&
                    static_cast<std::size_t>(position.x) < autoGrid[position.y].size() &&
                    autoGrid[position.y][position.x].has_value()) {
                    hasContent = true;
                }
            }
        }
        if (hasContent) {
            return TileLayerPassable(layer, position).cast<bool>();
        }
    }
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
        auto it = actorsRef.find(layerName);
        if (it != actorsRef.end()) {
            auto layerActors = it->second;
            for (auto &actor : layerActors) {
                if (getMapPosition(actor).cast<sf::Vector2i>() == pos) {
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
