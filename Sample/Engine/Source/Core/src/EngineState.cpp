#include <EngineState.hpp>

#include <algorithm>
#include <stdexcept>

bool GameRunning = true;
int CellSize = 32;
sf::Vector2u GameSize = {640u, 480u};
float Scale = 1.0f;
const sf::Vector2f ZeroVector2f = {0.0f, 0.0f};
const sf::Vector2i ZeroVector2i = {0, 0};
const sf::Vector2u ZeroVector2u = {0u, 0u};
const sf::Vector3f ZeroVector3f = {0.0f, 0.0f, 0.0f};
const sf::Vector3i ZeroVector3i = {0, 0, 0};
const sf::Vector3u ZeroVector3u = {0u, 0u, 0u};
const std::unordered_map<std::string, int> Direction = {
    {"DOWN", 0},
    {"LEFT", 1},
    {"RIGHT", 2},
    {"UP", 3},
};

float EngineState::getScale() const {
    return Scale;
}

void EngineState::setScale(float scale) {
    Scale = std::max(0.01f, scale);
}

sf::Vector2u EngineState::getGameSize() const {
    return GameSize;
}

void EngineState::setGameSize(const sf::Vector2u& size) {
    GameSize = size;
}

bool EngineState::getGameRunning() const {
    return GameRunning;
}

void EngineState::setGameRunning(bool running) {
    GameRunning = running;
}

int EngineState::getCellSize() const {
    return CellSize;
}

void EngineState::setCellSize(int cellSize) {
    CellSize = std::max(1, cellSize);
}

EngineState& engineState() {
    static EngineState state;
    return state;
}

void resetEngineState() noexcept {
    GameRunning = true;
    CellSize = 32;
    GameSize = {640u, 480u};
    Scale = 1.0f;
}

int oppositeDirection(int direction) {
    switch (direction) {
        case 0:
            return 3;
        case 1:
            return 2;
        case 2:
            return 1;
        case 3:
            return 0;
        default:
            throw std::invalid_argument(
                "direction must be an integer between 0 and 3");
    }
}
