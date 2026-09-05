#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

BIND_FUNCTION(name = "CanvasRenderStates")
LUDORK_ENGINE_API sf::RenderStates canvasRenderStates();

BIND_FUNCTION(name = "BuildPixelGridVertices")
LUDORK_ENGINE_API sf::VertexArray buildPixelGridVertices(
    const sf::Vector2f& origin, const sf::Vector2u& size);

LUDORK_ENGINE_API sf::RenderStates premultipliedRenderStates();

LUDORK_ENGINE_API sf::Vector2u nonZeroRenderTextureSize(
    const sf::Vector2u& size);
