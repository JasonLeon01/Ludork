#include <UI/UiResources.hpp>

#include <UI/UIState.hpp>

#include <algorithm>
#include <utility>

const std::shared_ptr<sf::Font>& UiResources::getDefaultFont() const {
    return defaultFont;
}

void UiResources::setDefaultFont(std::shared_ptr<sf::Font> font) {
    defaultFont = std::move(font);
}

int UiResources::getDefaultFontSize() const {
    return defaultFontSize;
}

void UiResources::setDefaultFontSize(int size) {
    defaultFontSize = std::max(1, size);
}

const std::optional<std::string>& UiResources::getDefaultWindowskinName()
    const {
    return defaultWindowskinName;
}

void UiResources::setDefaultWindowskinName(std::optional<std::string> name) {
    defaultWindowskinName = std::move(name);
}

void UiResources::reset() noexcept {
    defaultFont.reset();
    defaultFontSize = 32;
    defaultWindowskinName.reset();
}

UiResources& uiResources() {
    static UiResources resources;
    return resources;
}
