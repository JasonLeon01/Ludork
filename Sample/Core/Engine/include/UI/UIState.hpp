#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

BIND_MODULE_PROPERTY(name = "DefaultFontSize")
extern LUDORK_ENGINE_API int defaultFontSize;

BIND_MODULE_PROPERTY(name = "DefaultFont", metadata = false)
extern LUDORK_ENGINE_API std::shared_ptr<sf::Font> defaultFont;

BIND_MODULE_PROPERTY(name = "DefaultWindowskinName", metadata = false)
extern LUDORK_ENGINE_API std::optional<std::string> defaultWindowskinName;
