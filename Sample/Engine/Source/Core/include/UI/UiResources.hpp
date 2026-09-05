#pragma once

#include <EngineRuntimeApi.hpp>

#include <SFML/Graphics/Font.hpp>

#include <memory>
#include <optional>
#include <string>

class LUDORK_ENGINE_API UiResources {
public:
    const std::shared_ptr<sf::Font>& getDefaultFont() const;
    void setDefaultFont(std::shared_ptr<sf::Font> font);

    int getDefaultFontSize() const;
    void setDefaultFontSize(int size);

    const std::optional<std::string>& getDefaultWindowskinName() const;
    void setDefaultWindowskinName(std::optional<std::string> name);

    void reset() noexcept;
};

LUDORK_ENGINE_API UiResources& uiResources();
