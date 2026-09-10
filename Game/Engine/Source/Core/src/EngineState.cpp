#include <EngineState.hpp>

#include <algorithm>
#include <stdexcept>

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
    return scale_;
}

void EngineState::setScale(float scale) {
    scale_ = std::max(0.01f, scale);
}

sf::Vector2u EngineState::getGameSize() const {
    return gameSize_;
}

void EngineState::setGameSize(const sf::Vector2u& size) {
    gameSize_ = size;
}

bool EngineState::getGameRunning() const {
    return gameRunning_;
}

void EngineState::setGameRunning(bool running) {
    gameRunning_ = running;
}

void EngineState::reset() noexcept {
    gameRunning_ = true;
    gameSize_ = {640u, 480u};
    scale_ = 1.0f;
}

EngineState& engineState() {
    static EngineState state;
    return state;
}

void resetEngineState() noexcept {
    engineState().reset();
}

int getCellSize() {
    return EngineState::CellSize;
}

sf::Vector2u getGameSize() {
    return engineState().getGameSize();
}

float getScale() {
    return engineState().getScale();
}

bool isGameRunning() {
    return engineState().getGameRunning();
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
