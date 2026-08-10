#pragma once

#include <EngineRuntimeApi.hpp>

#include <SFML/System/Vector2.hpp>

class UiAssetInstance;

class LUDORK_ENGINE_API UiLayoutEngine {
public:
    static void reflow(UiAssetInstance& instance,
                       const sf::Vector2f& logicalSize);
};
