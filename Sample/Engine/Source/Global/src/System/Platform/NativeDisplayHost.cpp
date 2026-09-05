#include <System/NativeDisplayHost.hpp>

#include <mutex>
#include <utility>

namespace ludork::global::native_display_host {
namespace {

std::mutex hostMutex;
DisplayScaleRequestHandler displayScaleRequestHandler;
bool displayScaleConfigurable = false;
std::optional<sf::Vector2u> maximumWindowedSize;

}  // namespace

void setDisplayScaleHost(bool configurable,
                         const sf::Vector2u& hostMaximumWindowedSize,
                         DisplayScaleRequestHandler handler) {
    const std::lock_guard<std::mutex> lock(hostMutex);
    displayScaleConfigurable = configurable && static_cast<bool>(handler);
    maximumWindowedSize =
        displayScaleConfigurable && hostMaximumWindowedSize.x > 0 &&
                hostMaximumWindowedSize.y > 0
            ? std::optional<sf::Vector2u>(hostMaximumWindowedSize)
            : std::nullopt;
    displayScaleRequestHandler = displayScaleConfigurable
                                     ? std::move(handler)
                                     : DisplayScaleRequestHandler{};
}

void clearDisplayScaleHost() noexcept {
    const std::lock_guard<std::mutex> lock(hostMutex);
    displayScaleConfigurable = false;
    maximumWindowedSize.reset();
    displayScaleRequestHandler = {};
}

bool isDisplayScaleConfigurable() {
    const std::lock_guard<std::mutex> lock(hostMutex);
    return displayScaleConfigurable &&
           static_cast<bool>(displayScaleRequestHandler);
}

std::optional<sf::Vector2u> getMaximumWindowedSize() {
    const std::lock_guard<std::mutex> lock(hostMutex);
    return maximumWindowedSize;
}

void requestDisplayScale(float scale, const sf::Vector2u& gameSize) {
    DisplayScaleRequestHandler handler;
    {
        const std::lock_guard<std::mutex> lock(hostMutex);
        if (!displayScaleConfigurable || !displayScaleRequestHandler) {
            return;
        }
        handler = displayScaleRequestHandler;
    }
    handler(scale, gameSize);
}

}  // namespace ludork::global::native_display_host
