#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

BIND_LUA_HELPER(kind = "cast", path = "Cast")
BIND_LUA_HELPER(kind = "assert_type", path = "AssertType")
BIND_LUA_HELPER(kind = "eval", path = "Eval")

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
    static constexpr int CellSize = 32;

    float getScale() const;
    void setScale(float scale);
    sf::Vector2u getGameSize() const;
    void setGameSize(const sf::Vector2u& size);
    bool getGameRunning() const;
    void setGameRunning(bool running);
    void reset() noexcept;

private:
    bool gameRunning_ = true;
    sf::Vector2u gameSize_ = {640u, 480u};
    float scale_ = 1.0f;
};

LUDORK_ENGINE_API EngineState& engineState();

LUDORK_ENGINE_API void resetEngineState() noexcept;

BIND_FUNCTION(name = "GetCellSize", Pure = true)
LUDORK_ENGINE_API int getCellSize();

BIND_FUNCTION(name = "GetGameSize", Pure = true)
LUDORK_ENGINE_API sf::Vector2u getGameSize();

BIND_FUNCTION(name = "GetScale", Pure = true)
LUDORK_ENGINE_API float getScale();

BIND_FUNCTION(name = "IsGameRunning", Pure = true)
LUDORK_ENGINE_API bool isGameRunning();

BIND_FUNCTION(name = "OppositeDirection")
LUDORK_ENGINE_API int oppositeDirection(int direction);
