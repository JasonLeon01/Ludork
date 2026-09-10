#pragma once

#include <GlobalRuntimeApi.hpp>

#include <SFML/System/Vector2.hpp>

#include <functional>
#include <optional>

namespace ludork::global::native_display_host {

using DisplayScaleRequestHandler = std::function<void(float, sf::Vector2u)>;

LUDORK_GLOBAL_API void setDisplayScaleHost(
    bool configurable, const sf::Vector2u& maximumWindowedSize,
    DisplayScaleRequestHandler handler);
LUDORK_GLOBAL_API void clearDisplayScaleHost() noexcept;
LUDORK_GLOBAL_API bool isDisplayScaleConfigurable();
LUDORK_GLOBAL_API std::optional<sf::Vector2u> getMaximumWindowedSize();
LUDORK_GLOBAL_API void requestDisplayScale(float scale,
                                           const sf::Vector2u& gameSize);

}  // namespace ludork::global::native_display_host
