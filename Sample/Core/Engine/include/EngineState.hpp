#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>

#include <string>
#include <unordered_map>

BIND_LUA_HELPER(kind = "cast", path = "Cast")
BIND_LUA_HELPER(kind = "assert_type", path = "AssertType")
BIND_LUA_HELPER(kind = "eval", path = "Eval")

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API bool GameRunning;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API int CellSize;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API sf::Vector2u GameSize;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API float Scale;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API const sf::Vector2f ZeroVector2f;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API const sf::Vector2i ZeroVector2i;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API const sf::Vector2u ZeroVector2u;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API const sf::Vector3f ZeroVector3f;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API const sf::Vector3i ZeroVector3i;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API const sf::Vector3u ZeroVector3u;

BIND_MODULE_PROPERTY()
extern LUDORK_ENGINE_API const std::unordered_map<std::string, int> Direction;

class LUDORK_ENGINE_API EngineState {
public:
    float getScale() const;
    void setScale(float scale);
    sf::Vector2u getGameSize() const;
    void setGameSize(const sf::Vector2u& size);
    bool getGameRunning() const;
    void setGameRunning(bool running);
    int getCellSize() const;
    void setCellSize(int cellSize);
};

LUDORK_ENGINE_API EngineState& engineState();

LUDORK_ENGINE_API void resetEngineState() noexcept;

BIND_FUNCTION(name = "OppositeDirection")
LUDORK_ENGINE_API int oppositeDirection(int direction);
