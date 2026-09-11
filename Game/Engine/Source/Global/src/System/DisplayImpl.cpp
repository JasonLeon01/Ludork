#include "DisplayImpl.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"
#include "Platform/DesktopTextInputHost.hpp"
#include "Platform/EmbeddedTextInputHostImpl.hpp"
#if defined(SFML_SYSTEM_IOS)
#include "Platform/TextInputHostIOS.hpp"
#endif
#include <Input/TextInputService.hpp>
#include "Diagnostics/PerformanceProfiler.hpp"
#include <EngineState.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/WebViewHost.hpp>
#include <System/NativeDisplayHost.hpp>
#include <SystemConfigBase.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ludork::global::system_impl {

float DisplayImpl::windowFitScale(const sf::Vector2u& surfaceSize,
                                  const sf::Vector2u& gameSize) {
    const float scale = std::min(
        static_cast<float>(surfaceSize.x) / static_cast<float>(gameSize.x),
        static_cast<float>(surfaceSize.y) / static_cast<float>(gameSize.y));
    return std::max(0.01f, scale);
}

float DisplayImpl::effectiveRenderScale(float surfaceFitScale,
                                        float maximumRenderScale) {
    const float normalizedSurfaceFitScale = std::max(0.01f, surfaceFitScale);
    const float effectiveScale =
        maximumRenderScale > 0.0f
            ? std::min(normalizedSurfaceFitScale, maximumRenderScale)
            : normalizedSurfaceFitScale;
    return std::max(0.01f, effectiveScale);
}

sf::Vector2u DisplayImpl::scaledSize(const sf::Vector2u& gameSize,
                                     float scale) {
    const float normalizedScale = std::max(0.01f, scale);
    return {
        static_cast<unsigned int>(std::max(
            1.0f,
            std::floor(static_cast<float>(gameSize.x) * normalizedScale))),
        static_cast<unsigned int>(std::max(
            1.0f,
            std::floor(static_cast<float>(gameSize.y) * normalizedScale))),
    };
}

std::optional<float> DisplayImpl::getMaximumWindowedScale(
    const sf::Vector2u& gameSize) {
    if (gameSize.x == 0 || gameSize.y == 0 || isEmbeddedDisplay()) {
        return std::nullopt;
    }
    std::optional<sf::Vector2u> maximumSize;
    if (isMobileDisplay()) {
        maximumSize =
            ludork::global::native_display_host::getMaximumWindowedSize();
    } else {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        const sf::WindowHandle windowHandle =
            window_ != nullptr && window_->isOpen() ? window_->getNativeHandle()
                                                    : sf::WindowHandle{};
        maximumSize = ludork::global::getMaximumWindowedClientSize(
            windowHandle, ludork::global::runtimeWindowStyle());
    }
    if (!maximumSize.has_value() || maximumSize->x == 0 ||
        maximumSize->y == 0) {
        return std::nullopt;
    }
    return std::min(
        static_cast<float>(maximumSize->x) / static_cast<float>(gameSize.x),
        static_cast<float>(maximumSize->y) / static_cast<float>(gameSize.y));
}

sf::Vector2u DisplayImpl::getGameSize() {
    return engineState().getGameSize();
}

void DisplayImpl::setGameSize(const sf::Vector2u& gameSize) {
    engineState().setGameSize(gameSize);
}

void DisplayImpl::finishInitialization(const sf::Vector2u& renderSize) {
    observedWindowSize_ = window_->getSize();
    observedWindowClientSize_ =
        desktopFullscreen_
            ? std::nullopt
            : ludork::global::getWindowedClientSize(window_->getNativeHandle());
    updateWindowViewport(renderSize);
    if (isMobileDisplay() && isDisplayScaleConfigurable()) {
        ludork::global::native_display_host::requestDisplayScale(
            SystemConfigBase::getConfiguredScale(), getGameSize());
    }
}

void DisplayImpl::prepareInitialization(const std::string& title,
                                        const sf::Vector2u& gameSize,
                                        const std::string& iconPath,
                                        const std::string& cursorPath) {
    if (gameSize.x == 0 || gameSize.y == 0) {
        throw std::invalid_argument("Game size must be non-zero");
    }
    if (window_ != nullptr) {
        throw std::logic_error("Display has already been initialized");
    }
    windowTitle_ = title;
    windowIconPath_ = iconPath;
    windowCursorPath_ = cursorPath;
    windowContextSettings_ = {};
    windowContextSettings_.antiAliasingLevel =
        static_cast<unsigned int>(SystemConfigBase::getAntiAliasingLevel());
#if defined(SFML_OPENGL_ES)
    windowContextSettings_.majorVersion = 3;
    windowContextSettings_.minorVersion = 0;
#endif
    setGameSize(gameSize);
}

void DisplayImpl::createDisplayWindow() {
    const ludork::global::RuntimeLaunchOptions& launchOptions =
        ludork::global::runtimeLaunchOptions();
    std::shared_ptr<sf::RenderWindow> window;
    float surfaceFitScale = 1.0f;
    inputService().setUseInjectedMouseOnly(false);

    if (isEmbeddedDisplay()) {
#if defined(_WIN32)
        if (!launchOptions.hostWindowHandle.has_value()) {
            throw std::invalid_argument("Embedded window handle is required");
        }
        window = std::make_shared<sf::RenderWindow>(
            reinterpret_cast<sf::WindowHandle>(
                launchOptions.hostWindowHandle.value()),
            windowContextSettings_);
        surfaceFitScale = windowFitScale(window->getSize());
        inputService().setUseInjectedMouseOnly(true);
#else
        throw std::runtime_error(
            "Embedded window mode is only supported on Windows");
#endif
    } else if (isMobileDisplay()) {
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode::getDesktopMode(), windowTitle_,
            ludork::global::runtimeWindowStyle(), sf::State::Fullscreen,
            windowContextSettings_);
        surfaceFitScale = windowFitScale(window->getSize());
    } else {
        const float configuredScale = SystemConfigBase::getConfiguredScale();
        desktopFullscreen_ = configuredScale == 0.0f;
        const sf::Vector2u windowSize =
            desktopFullscreen_ ? sf::VideoMode::getDesktopMode().size
                               : windowSizeForScale(configuredScale);
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode(windowSize), windowTitle_,
            desktopFullscreen_ ? sf::Style::None
                               : ludork::global::runtimeWindowStyle(),
            sf::State::Windowed, windowContextSettings_);
        const std::optional<sf::Vector2u> clientSize =
            desktopFullscreen_ ? std::nullopt
                               : ludork::global::getWindowedClientSize(
                                     window->getNativeHandle());
        surfaceFitScale =
            windowFitScale(clientSize.value_or(window->getSize()));
    }

    surfaceFitScale_ = surfaceFitScale;
    engineState().setScale(effectiveRenderScale(surfaceFitScale));
#if defined(SFML_OPENGL_ES)
    if (window->getSettings().majorVersion < 3) {
        throw std::runtime_error(
            "Ludork requires an OpenGL ES 3.0 context, but OpenGL ES " +
            std::to_string(window->getSettings().majorVersion) + "." +
            std::to_string(window->getSettings().minorVersion) +
            " was created");
    }
#endif
    initWindow(window);
}

void DisplayImpl::initializeInput() {
    ludork::runtime::webview::attachWindow(window_->getNativeHandle());
    setInputMethodDisabled(true);
#if defined(SFML_SYSTEM_IOS)
    ludork::engine::text_input::service().setHost(
        ludork::global::createIosTextInputHost(window_->getNativeHandle()));
#elif defined(_WIN32) || (defined(__APPLE__) && !defined(LUDORK_MOBILE))
    if (isEmbeddedDisplay()) {
        ludork::engine::text_input::service().setHost(
            std::make_shared<ludork::global::EmbeddedTextInputHostImpl>());
    } else {
        ludork::engine::text_input::service().setHost(
            ludork::global::createDesktopTextInputHost(*window_));
    }
#endif
    inputService().initializeNativePolling();
}

void DisplayImpl::initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    if (window == nullptr) {
        throw std::invalid_argument("System window cannot be nil");
    }
    ludork::runtime::webview::detachWindow();
    {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        window_ = window;
    }
    applyWindowPresentationSettings();
}

std::shared_ptr<sf::RenderWindow> DisplayImpl::getWindow() {
    const std::lock_guard<std::mutex> lock(windowMutex_);
    return window_;
}

bool DisplayImpl::isEmbeddedDisplay() {
    return ludork::global::runtimeLaunchOptions().windowMode ==
           ludork::global::RuntimeWindowMode::Embedded;
}

bool DisplayImpl::isMobileDisplay() {
#if defined(LUDORK_MOBILE)
    return true;
#else
    return false;
#endif
}

bool DisplayImpl::isDisplayScaleConfigurable() {
    if (isEmbeddedDisplay()) {
        return false;
    }
    if (!isMobileDisplay()) {
        return true;
    }
    return ludork::global::native_display_host::isDisplayScaleConfigurable();
}

float DisplayImpl::windowFitScale(const sf::Vector2u& size) {
    return windowFitScale(size, getGameSize());
}

float DisplayImpl::effectiveRenderScale(float surfaceFitScale) {
    return effectiveRenderScale(surfaceFitScale,
                                SystemConfigBase::getMaximumRenderScale());
}

sf::Vector2u DisplayImpl::windowSizeForScale(float scale) {
    return scaledSize(getGameSize(), scale);
}

sf::Vector2u DisplayImpl::renderSizeForScale(float scale) {
    return windowSizeForScale(scale);
}

void DisplayImpl::applyWindowPresentationSettings() {
    if (window_ == nullptr) {
        return;
    }
    window_->setFramerateLimit(static_cast<unsigned int>(
        std::max(0, SystemConfigBase::getFrameRate())));
    window_->setVerticalSyncEnabled(SystemConfigBase::getVerticalSync());
    window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                       : sf::Color::Black);
    if (!isMobileDisplay() && !windowIconPath_.empty()) {
        std::unique_ptr<ludork::runtime::AssetInputStream> iconStream =
            ludork::runtime::assetStore().open(windowIconPath_);
        sf::Image icon;
        if (!icon.loadFromStream(*iconStream)) {
            throw std::runtime_error("Failed to load window icon: " +
                                     windowIconPath_);
        }
        window_->setIcon(icon);
    }
    cursor_.reset();
    if (isMobileDisplay() || windowCursorPath_.empty()) {
        return;
    }
    if (!ludork::runtime::assetStore().exists(windowCursorPath_)) {
        return;
    }
    try {
        std::unique_ptr<ludork::runtime::AssetInputStream> cursorStream =
            ludork::runtime::assetStore().open(windowCursorPath_);
        sf::Image cursorImage;
        if (!cursorImage.loadFromStream(*cursorStream)) {
            throw std::runtime_error("Failed to load cursor image");
        }
        cursor_ = std::make_unique<sf::Cursor>(
            cursorImage.getPixelsPtr(), cursorImage.getSize(), sf::Vector2u{});
        window_->setMouseCursor(*cursor_);
    } catch (const std::exception& exception) {
        std::cerr << "Failed to create cursor from " << windowCursorPath_
                  << ": " << exception.what() << '\n';
    }
}

void DisplayImpl::recreateDesktopWindow(bool fullscreen,
                                        const sf::Vector2u& size) {
    const std::lock_guard<std::mutex> lock(windowMutex_);
    if (window_ == nullptr || isEmbeddedDisplay() || isMobileDisplay()) {
        return;
    }
    ludork::runtime::webview::detachWindow();
    ludork::engine::text_input::service().setHost(nullptr);
    ludork::global::restoreNativeInputMethod();
    window_->create(
        sf::VideoMode(size), windowTitle_,
        fullscreen ? sf::Style::None : ludork::global::runtimeWindowStyle(),
        sf::State::Windowed, windowContextSettings_);
    desktopFullscreen_ = fullscreen;
    if (fullscreen) {
        window_->setPosition({0, 0});
    }
    applyWindowPresentationSettings();
    setInputMethodDisabled(inputMethodDisabled_);
    inputService().onWindowRecreated(*window_);
    initializeInput();
}

void DisplayImpl::replaceWindowedDesktopWindow(
    const sf::Vector2u& size,
    const ludork::global::WindowedFramePlacement* placement) {
    const std::shared_ptr<sf::RenderWindow> previousWindow = getWindow();
    if (previousWindow == nullptr || isEmbeddedDisplay() || isMobileDisplay()) {
        return;
    }
    ludork::runtime::webview::detachWindow();
    ludork::engine::text_input::service().setHost(nullptr);
    ludork::global::restoreNativeInputMethod();
    const std::shared_ptr<sf::RenderWindow> replacement =
        std::make_shared<sf::RenderWindow>(sf::VideoMode(size), windowTitle_,
                                           ludork::global::runtimeWindowStyle(),
                                           sf::State::Windowed,
                                           windowContextSettings_);
    {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        window_ = replacement;
        desktopFullscreen_ = false;
        if (placement != nullptr) {
            ludork::global::setWindowedFramePlacement(
                window_->getNativeHandle(), *placement);
        }
        applyWindowPresentationSettings();
        setInputMethodDisabled(inputMethodDisabled_);
        inputService().onWindowRecreated(*window_);
        initializeInput();
    }
}

void DisplayImpl::updateWindowViewport(const sf::Vector2u& renderSize) {
    if (window_ == nullptr) {
        return;
    }
    const sf::Vector2u windowSize = window_->getSize();
    if (windowSize.x == 0 || windowSize.y == 0 || renderSize.x == 0 ||
        renderSize.y == 0) {
        return;
    }
    const float fit = std::min(
        static_cast<float>(windowSize.x) / static_cast<float>(renderSize.x),
        static_cast<float>(windowSize.y) / static_cast<float>(renderSize.y));
    const sf::Vector2f contentSize{
        static_cast<float>(renderSize.x) * fit,
        static_cast<float>(renderSize.y) * fit,
    };
    const sf::Vector2f windowSizeFloat{static_cast<float>(windowSize.x),
                                       static_cast<float>(windowSize.y)};
    const sf::Vector2f offset = (windowSizeFloat - contentSize) / 2.0f;
    sf::View view(sf::Vector2f(renderSize) / 2.0f, sf::Vector2f(renderSize));
    view.setViewport(sf::FloatRect(
        {offset.x / windowSizeFloat.x, offset.y / windowSizeFloat.y},
        {contentSize.x / windowSizeFloat.x,
         contentSize.y / windowSizeFloat.y}));
    window_->setView(view);
    const sf::Vector2i viewportPosition{
        static_cast<int>(std::ceil(offset.x)),
        static_cast<int>(std::ceil(offset.y)),
    };
    const sf::Vector2i viewportSize{
        std::max(0, static_cast<int>(std::floor(contentSize.x))),
        std::max(0, static_cast<int>(std::floor(contentSize.y))),
    };
    inputService().setPointerViewport(
        sf::IntRect(viewportPosition, viewportSize));
}

std::optional<float> DisplayImpl::applyConfiguredScale(float scale) {
    if (window_ == nullptr || isEmbeddedDisplay()) {
        return std::nullopt;
    }
    if (isMobileDisplay()) {
        if (isDisplayScaleConfigurable()) {
            ludork::global::native_display_host::requestDisplayScale(
                scale, getGameSize());
        }
        return std::nullopt;
    }
    const bool fullscreen = scale == 0.0f;
    sf::Vector2u targetSize = fullscreen ? sf::VideoMode::getDesktopMode().size
                                         : windowSizeForScale(scale);
    std::optional<sf::Vector2u> clientSize;
    if (fullscreen != desktopFullscreen_) {
        recreateDesktopWindow(fullscreen, targetSize);
        if (!fullscreen) {
            clientSize = ludork::global::getWindowedClientSize(
                window_->getNativeHandle());
        }
    } else if (!fullscreen) {
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
        const std::optional<ludork::global::WindowedFramePlacement> placement =
            ludork::global::getWindowedFramePlacement(
                window_->getNativeHandle());
        replaceWindowedDesktopWindow(
            targetSize, placement.has_value() ? &*placement : nullptr);
#else
        window_->setSize(targetSize);
#endif
        clientSize =
            ludork::global::getWindowedClientSize(window_->getNativeHandle());
    }
    observedWindowSize_ = window_->getSize();
    observedWindowClientSize_ = clientSize;
    pendingResizeScale_.reset();
    return windowFitScale(clientSize.value_or(observedWindowSize_));
}

std::optional<float> DisplayImpl::observeWindowResize(
    const sf::Vector2u& renderSize) {
    if (window_ == nullptr) {
        return std::nullopt;
    }
    const sf::Vector2u size = window_->getSize();
    const auto now = std::chrono::steady_clock::now();
    if (size != observedWindowSize_) {
        const std::optional<sf::Vector2u> clientSize =
            desktopFullscreen_ ? std::nullopt
                               : ludork::global::getWindowedClientSize(
                                     window_->getNativeHandle());
        const bool clientSizeChanged =
            !clientSize.has_value() || clientSize != observedWindowClientSize_;
        observedWindowSize_ = size;
        observedWindowClientSize_ = clientSize;
        if (clientSizeChanged) {
            pendingResizeScale_ =
                windowFitScale(clientSize.value_or(observedWindowSize_));
            lastResizeTime_ = now;
        }
        updateWindowViewport(renderSize);
    }
    if (!pendingResizeScale_.has_value() ||
        now - lastResizeTime_ < std::chrono::milliseconds(150)) {
        return std::nullopt;
    }
    float scale = *pendingResizeScale_;
    pendingResizeScale_.reset();
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !desktopFullscreen_) {
        const std::optional<sf::Vector2u> clientSize =
            ludork::global::getWindowedClientSize(window_->getNativeHandle());
        if (clientSize.has_value()) {
            const std::optional<ludork::global::WindowedFramePlacement>
                placement = ludork::global::getWindowedFramePlacement(
                    window_->getNativeHandle());
            replaceWindowedDesktopWindow(
                *clientSize, placement.has_value() ? &*placement : nullptr);
            observedWindowSize_ = window_->getSize();
            const std::optional<sf::Vector2u> replacedClientSize =
                ludork::global::getWindowedClientSize(
                    window_->getNativeHandle());
            observedWindowClientSize_ = replacedClientSize;
            scale = windowFitScale(replacedClientSize.value_or(*clientSize));
        }
    }
#endif
    return scale;
}

void DisplayImpl::setInputMethodDisabled(bool disabled) {
    inputMethodDisabled_ = disabled;
    if (window_ == nullptr ||
        ludork::global::runtimeLaunchOptions().windowMode ==
            ludork::global::RuntimeWindowMode::Embedded) {
        return;
    }
    ludork::global::setNativeInputMethodDisabled(
        window_->getNativeHandle(),
        disabled && !ludork::engine::text_input::service().isEditing());
}

void DisplayImpl::present() {
    if (window_ == nullptr) {
        return;
    }
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !desktopFullscreen_) {
        const sf::Vector2u windowSize = window_->getSize();
        const bool liveResizing = ludork::global::isNativeWindowLiveResizing(
            window_->getNativeHandle());
        if (liveResizing || windowSize != observedWindowSize_) {
            const std::optional<sf::Vector2u> clientSize =
                ludork::global::getWindowedClientSize(
                    window_->getNativeHandle());
            if (liveResizing || !clientSize.has_value() ||
                clientSize != observedWindowClientSize_) {
                pendingResizeScale_ =
                    windowFitScale(clientSize.value_or(windowSize));
                lastResizeTime_ = std::chrono::steady_clock::now();
            }
            return;
        }
        if (pendingResizeScale_.has_value()) {
            return;
        }
    }
#endif
    if (PerformanceProfiler::isEnabled()) {
        const auto presentStart = std::chrono::steady_clock::now();
        window_->display();
        PerformanceProfiler::addPresentWait(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - presentStart)
                .count());
    } else {
        window_->display();
    }
}

void DisplayImpl::clearWindow() {
    if (window_ != nullptr) {
        window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                           : sf::Color::Black);
    }
}

bool DisplayImpl::isOpen() const {
    const std::lock_guard<std::mutex> lock(windowMutex_);
    return window_ != nullptr && window_->isOpen();
}

void DisplayImpl::requestConfiguredScale(float scale) {
    const std::lock_guard<std::mutex> lock(pendingSettingsMutex_);
    pendingConfiguredScale_ = scale;
}

std::optional<float> DisplayImpl::takeConfiguredScale() {
    std::optional<float> scale;
    {
        const std::lock_guard<std::mutex> lock(pendingSettingsMutex_);
        scale = std::exchange(pendingConfiguredScale_, std::nullopt);
    }
    if (isMobileDisplay() && !isEmbeddedDisplay() &&
        ludork::global::native_display_host::takeDisplayScaleRestoreRequest() &&
        !scale.has_value()) {
        scale = SystemConfigBase::getConfiguredScale();
    }
    return scale;
}

void DisplayImpl::requestRenderTargetRebuild() {
    const std::lock_guard<std::mutex> lock(pendingSettingsMutex_);
    pendingRenderTargetRebuild_ = true;
}

bool DisplayImpl::takeRenderTargetRebuild() {
    const std::lock_guard<std::mutex> lock(pendingSettingsMutex_);
    return std::exchange(pendingRenderTargetRebuild_, false);
}

float DisplayImpl::getSurfaceFitScale() const {
    return surfaceFitScale_;
}

void DisplayImpl::setSurfaceFitScale(float scale) {
    surfaceFitScale_ = scale;
}

void DisplayImpl::applyFrameRate() {
    const std::shared_ptr<sf::RenderWindow> window = getWindow();
    if (window != nullptr) {
        window->setFramerateLimit(static_cast<unsigned int>(
            std::max(0, SystemConfigBase::getFrameRate())));
    }
}

void DisplayImpl::applyVerticalSync() {
    const std::shared_ptr<sf::RenderWindow> window = getWindow();
    if (window != nullptr) {
        window->setVerticalSyncEnabled(SystemConfigBase::getVerticalSync());
    }
}

void DisplayImpl::reset() {
    {
        const std::lock_guard<std::mutex> lock(pendingSettingsMutex_);
        pendingConfiguredScale_.reset();
        pendingRenderTargetRebuild_ = false;
    }
    pendingResizeScale_.reset();
    surfaceFitScale_ = 1.0f;
    observedWindowSize_ = {};
    observedWindowClientSize_.reset();
    desktopFullscreen_ = false;
    inputMethodDisabled_ = true;
}

void DisplayImpl::shutdown() noexcept {
    ludork::runtime::webview::detachWindow();
    ludork::engine::text_input::service().setHost(nullptr);
    ludork::global::restoreNativeInputMethod();
    std::shared_ptr<sf::RenderWindow> previousWindow;
    {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        previousWindow = std::move(window_);
    }
    previousWindow.reset();
    cursor_.reset();
    windowTitle_.clear();
    windowIconPath_.clear();
    windowCursorPath_.clear();
    windowContextSettings_ = {};
    lastResizeTime_ = {};
    reset();
}

}  // namespace ludork::global::system_impl
