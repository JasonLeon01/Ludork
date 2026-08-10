#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/System/Vector2.hpp>

BIND_FUNCTION(name = "CanvasRenderStates")
LUDORK_ENGINE_API sf::RenderStates canvasRenderStates();

LUDORK_ENGINE_API sf::RenderStates premultipliedRenderStates();

LUDORK_ENGINE_API sf::Vector2u nonZeroRenderTextureSize(
    const sf::Vector2u& size);
