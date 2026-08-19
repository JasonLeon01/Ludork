#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>

#include <memory>
#include <optional>
#include <string>

BIND_MODULE_PROPERTY(name = "DefaultFontSize")
extern LUDORK_ENGINE_API int defaultFontSize;

BIND_MODULE_PROPERTY(name = "DefaultFont", metadata = false)
extern LUDORK_ENGINE_API std::shared_ptr<sf::Font> defaultFont;

BIND_MODULE_PROPERTY(name = "DefaultWindowskinName", metadata = false)
extern LUDORK_ENGINE_API std::optional<std::string> defaultWindowskinName;
