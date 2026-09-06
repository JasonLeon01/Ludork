#include "DisplayImpl.hpp"
#include "SystemImpl.hpp"
#include "LifecycleImpl.hpp"
#include "FramePipelineImpl.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"
#include <EngineState.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>
#include <Manager/ShaderManager.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>
#include <System/NativeDisplayHost.hpp>
#include <SystemConfigBase.hpp>
#include <Utils/Inner.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace ludork::global::system_display_impl {

bool viewsEqual(const sf::View& left, const sf::View& right) {
    return left.getCenter() == right.getCenter() &&
           left.getSize() == right.getSize() &&
           left.getRotation() == right.getRotation() &&
           left.getViewport() == right.getViewport() &&
           left.getScissor() == right.getScissor();
}

float windowFitScale(const sf::Vector2u& surfaceSize,
                     const sf::Vector2u& gameSize) {
    const float scale = std::min(
        static_cast<float>(surfaceSize.x) / static_cast<float>(gameSize.x),
        static_cast<float>(surfaceSize.y) / static_cast<float>(gameSize.y));
    return std::max(0.01f, scale);
}

float effectiveRenderScale(float surfaceFitScale, float maximumRenderScale) {
    const float normalizedSurfaceFitScale = std::max(0.01f, surfaceFitScale);
    const float effectiveScale =
        maximumRenderScale > 0.0f
            ? std::min(normalizedSurfaceFitScale, maximumRenderScale)
            : normalizedSurfaceFitScale;
    return std::max(0.01f, effectiveScale);
}

sf::Vector2u scaledSize(const sf::Vector2u& gameSize, float scale) {
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

std::optional<float> getMaximumWindowedScale(const sf::Vector2u& gameSize) {
    if (gameSize.x == 0 || gameSize.y == 0 ||
        ludork::global::system_display_impl::isEmbeddedDisplay()) {
        return std::nullopt;
    }
    std::optional<sf::Vector2u> maximumSize;
    if (ludork::global::system_display_impl::isMobileDisplay()) {
        maximumSize =
            ludork::global::native_display_host::getMaximumWindowedSize();
    } else {
        ludork::global::system_impl::DisplayImpl& display =
            ludork::global::system_impl::impl().display;
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        const sf::WindowHandle windowHandle =
            display.window_ != nullptr && display.window_->isOpen()
                ? display.window_->getNativeHandle()
                : sf::WindowHandle{};
        maximumSize =
            ludork::global::getMaximumWindowedClientSize(windowHandle);
    }
    if (!maximumSize.has_value() || maximumSize->x == 0 ||
        maximumSize->y == 0) {
        return std::nullopt;
    }
    return std::min(
        static_cast<float>(maximumSize->x) / static_cast<float>(gameSize.x),
        static_cast<float>(maximumSize->y) / static_cast<float>(gameSize.y));
}

sf::Vector2u getGameSize() {
    return engineState().getGameSize();
}

void setGameSize(const sf::Vector2u& gameSize) {
    engineState().setGameSize(gameSize);
}

void initializeDisplay(const std::string& title, const sf::Vector2u& gameSize,
                       const std::string& iconPath,
                       const std::string& cursorPath) {
    if (gameSize.x == 0 || gameSize.y == 0) {
        throw std::invalid_argument("Game size must be non-zero");
    }
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    if (display.window_ != nullptr) {
        throw std::logic_error("Display has already been initialized");
    }
    const ludork::global::RuntimeLaunchOptions& launchOptions =
        ludork::global::runtimeLaunchOptions();
    display.windowTitle_ = title;
    display.windowIconPath_ = iconPath;
    display.windowCursorPath_ = cursorPath;
    display.windowContextSettings_ = {};
    display.windowContextSettings_.antiAliasingLevel =
        static_cast<unsigned int>(SystemConfigBase::getAntiAliasingLevel());
#if defined(SFML_SYSTEM_IOS)
    display.windowContextSettings_.majorVersion = 3;
    display.windowContextSettings_.minorVersion = 0;
#endif
    std::shared_ptr<sf::RenderWindow> window;
    float surfaceFitScale = 1.0f;

    ludork::global::system_display_impl::setGameSize(gameSize);
    ludork::global::system_lifecycle_impl::setDebugMode(launchOptions.editor);
    inputService().setUseInjectedMouseOnly(false);

    if (ludork::global::system_display_impl::isEmbeddedDisplay()) {
#if defined(_WIN32)
        if (!launchOptions.hostWindowHandle.has_value()) {
            throw std::invalid_argument("Embedded window handle is required");
        }
        window = std::make_shared<sf::RenderWindow>(
            reinterpret_cast<sf::WindowHandle>(
                launchOptions.hostWindowHandle.value()),
            display.windowContextSettings_);
        surfaceFitScale = ludork::global::system_display_impl::windowFitScale(
            window->getSize());
        inputService().setUseInjectedMouseOnly(true);
#else
        throw std::runtime_error(
            "Embedded window mode is only supported on Windows");
#endif
    } else if (ludork::global::system_display_impl::isMobileDisplay()) {
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode::getDesktopMode(), title, sf::Style::Default,
            sf::State::Fullscreen, display.windowContextSettings_);
        surfaceFitScale = ludork::global::system_display_impl::windowFitScale(
            window->getSize());
    } else {
        const float configuredScale = SystemConfigBase::getConfiguredScale();
        display.desktopFullscreen_ = configuredScale == 0.0f;
        const sf::Vector2u windowSize =
            display.desktopFullscreen_
                ? sf::VideoMode::getDesktopMode().size
                : ludork::global::system_display_impl::windowSizeForScale(
                      configuredScale);
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode(windowSize), title,
            display.desktopFullscreen_ ? sf::Style::None : sf::Style::Default,
            sf::State::Windowed, display.windowContextSettings_);
        const std::optional<sf::Vector2u> clientSize =
            display.desktopFullscreen_ ? std::nullopt
                                       : ludork::global::getWindowedClientSize(
                                             window->getNativeHandle());
        surfaceFitScale = ludork::global::system_display_impl::windowFitScale(
            clientSize.value_or(window->getSize()));
    }

    display.surfaceFitScale_ = surfaceFitScale;
    engineState().setScale(
        ludork::global::system_display_impl::effectiveRenderScale(
            surfaceFitScale));
#if defined(SFML_SYSTEM_IOS)
    if (window->getSettings().majorVersion < 3) {
        throw std::runtime_error(
            "iOS requires an OpenGL ES 3.0 context, but OpenGL ES " +
            std::to_string(window->getSettings().majorVersion) + "." +
            std::to_string(window->getSettings().minorVersion) +
            " was created");
    }
#endif
    ludork::global::system_display_impl::initWindow(window);
    if (ludork::global::system_frame_pipeline_impl::shadersAvailable()) {
        framePipeline.transitionShader_ =
            ShaderManager::load("/Game/Assets/Shaders/Global/Transition.frag",
                                sf::Shader::Type::Fragment);
    } else {
        framePipeline.transitionShader_.reset();
        warnOnce("System.transitionShader",
                 "Shaders are unavailable; skipped loading transition shader");
    }
    ludork::global::system_display_impl::setInputMethodDisabled(true);
    inputService().initializeNativePolling();
    ludork::global::system_frame_pipeline_impl::initCanvas(
        ludork::global::system_display_impl::renderSizeForScale(
            SystemConfigBase::getScale()));
    display.observedWindowSize_ = display.window_->getSize();
    display.observedWindowClientSize_ =
        display.desktopFullscreen_ ? std::nullopt
                                   : ludork::global::getWindowedClientSize(
                                         display.window_->getNativeHandle());
    ludork::global::system_display_impl::updateWindowViewport();
    if (ludork::global::system_display_impl::isMobileDisplay() &&
        ludork::global::system_display_impl::isDisplayScaleConfigurable()) {
        ludork::global::native_display_host::requestDisplayScale(
            SystemConfigBase::getConfiguredScale(), gameSize);
    }
}

void initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    if (window == nullptr) {
        throw std::invalid_argument("System window cannot be nil");
    }
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    {
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        display.window_ = window;
    }
    ludork::global::system_display_impl::applyWindowPresentationSettings();
}

std::shared_ptr<sf::RenderWindow> getWindow() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    return display.window_;
}

bool isEmbeddedDisplay() {
    return ludork::global::runtimeLaunchOptions().windowMode ==
           ludork::global::RuntimeWindowMode::Embedded;
}

bool isMobileDisplay() {
#if defined(LUDORK_MOBILE)
    return true;
#else
    return false;
#endif
}

bool isDisplayScaleConfigurable() {
    if (ludork::global::system_display_impl::isEmbeddedDisplay()) {
        return false;
    }
    if (!ludork::global::system_display_impl::isMobileDisplay()) {
        return true;
    }
    return ludork::global::native_display_host::isDisplayScaleConfigurable();
}

float windowFitScale(const sf::Vector2u& size) {
    return ludork::global::system_display_impl::windowFitScale(
        size, ludork::global::system_display_impl::getGameSize());
}

float effectiveRenderScale(float surfaceFitScale) {
    return ludork::global::system_display_impl::effectiveRenderScale(
        surfaceFitScale, SystemConfigBase::getMaximumRenderScale());
}

sf::Vector2u windowSizeForScale(float scale) {
    return ludork::global::system_display_impl::scaledSize(
        ludork::global::system_display_impl::getGameSize(), scale);
}

sf::Vector2u renderSizeForScale(float scale) {
    return ludork::global::system_display_impl::windowSizeForScale(scale);
}

void applyWindowPresentationSettings() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.window_ == nullptr) {
        return;
    }
    display.window_->setFramerateLimit(static_cast<unsigned int>(
        std::max(0, SystemConfigBase::getFrameRate())));
    display.window_->setVerticalSyncEnabled(
        SystemConfigBase::getVerticalSync());
    display.window_->clear(
        ludork::global::system_display_impl::isEmbeddedDisplay()
            ? sf::Color::Transparent
            : sf::Color::Black);
    if (!ludork::global::system_display_impl::isMobileDisplay() &&
        !display.windowIconPath_.empty()) {
        std::unique_ptr<ludork::runtime::AssetInputStream> iconStream =
            ludork::runtime::assetStore().open(display.windowIconPath_);
        sf::Image icon;
        if (!icon.loadFromStream(*iconStream)) {
            throw std::runtime_error("Failed to load window icon: " +
                                     display.windowIconPath_);
        }
        display.window_->setIcon(icon);
    }
    display.cursor_.reset();
    if (ludork::global::system_display_impl::isMobileDisplay() ||
        display.windowCursorPath_.empty()) {
        return;
    }
    if (!ludork::runtime::assetStore().exists(display.windowCursorPath_)) {
        return;
    }
    try {
        std::unique_ptr<ludork::runtime::AssetInputStream> cursorStream =
            ludork::runtime::assetStore().open(display.windowCursorPath_);
        sf::Image cursorImage;
        if (!cursorImage.loadFromStream(*cursorStream)) {
            throw std::runtime_error("Failed to load cursor image");
        }
        display.cursor_ = std::make_unique<sf::Cursor>(
            cursorImage.getPixelsPtr(), cursorImage.getSize(), sf::Vector2u{});
        display.window_->setMouseCursor(*display.cursor_);
    } catch (const std::exception& exception) {
        std::cerr << "Failed to create cursor from "
                  << display.windowCursorPath_ << ": " << exception.what()
                  << '\n';
    }
}

void recreateDesktopWindow(bool fullscreen, const sf::Vector2u& size) {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    if (display.window_ == nullptr ||
        ludork::global::system_display_impl::isEmbeddedDisplay() ||
        ludork::global::system_display_impl::isMobileDisplay()) {
        return;
    }
    ludork::global::restoreNativeInputMethod();
    display.window_->create(sf::VideoMode(size), display.windowTitle_,
                            fullscreen ? sf::Style::None : sf::Style::Default,
                            sf::State::Windowed,
                            display.windowContextSettings_);
    display.desktopFullscreen_ = fullscreen;
    if (fullscreen) {
        display.window_->setPosition({0, 0});
    }
    ludork::global::system_display_impl::applyWindowPresentationSettings();
    ludork::global::system_display_impl::setInputMethodDisabled(
        display.inputMethodDisabled_);
    inputService().onWindowRecreated(*display.window_);
    inputService().initializeNativePolling();
}

void replaceWindowedDesktopWindow(
    const sf::Vector2u& size,
    const ludork::global::WindowedFramePlacement* placement) {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    const std::shared_ptr<sf::RenderWindow> previousWindow =
        ludork::global::system_display_impl::getWindow();
    if (previousWindow == nullptr ||
        ludork::global::system_display_impl::isEmbeddedDisplay() ||
        ludork::global::system_display_impl::isMobileDisplay()) {
        return;
    }
    ludork::global::restoreNativeInputMethod();
    const std::shared_ptr<sf::RenderWindow> replacement =
        std::make_shared<sf::RenderWindow>(
            sf::VideoMode(size), display.windowTitle_, sf::Style::Default,
            sf::State::Windowed, display.windowContextSettings_);
    {
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        display.window_ = replacement;
        display.desktopFullscreen_ = false;
        if (placement != nullptr) {
            ludork::global::setWindowedFramePlacement(
                display.window_->getNativeHandle(), *placement);
        }
        ludork::global::system_display_impl::applyWindowPresentationSettings();
        ludork::global::system_display_impl::setInputMethodDisabled(
            display.inputMethodDisabled_);
        inputService().onWindowRecreated(*display.window_);
        inputService().initializeNativePolling();
    }
}

void updateWindowViewport() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.window_ == nullptr || display.canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u windowSize = display.window_->getSize();
    const sf::Vector2u renderSize = display.canvas_->getSize();
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
    display.window_->setView(view);
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

void rebuildDisplayTargets(float surfaceFitScale) {
    const float normalizedSurfaceFitScale = std::max(0.01f, surfaceFitScale);
    const float renderScale =
        ludork::global::system_display_impl::effectiveRenderScale(
            normalizedSurfaceFitScale);
    const sf::Vector2u size =
        ludork::global::system_display_impl::renderSizeForScale(renderScale);
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    display.surfaceFitScale_ = normalizedSurfaceFitScale;
    if (display.canvas_ != nullptr && display.canvas_->getSize() == size &&
        engineState().getScale() == renderScale) {
        ludork::global::system_display_impl::updateWindowViewport();
        return;
    }
    std::optional<sf::Image> transitionImage;
    if (framePipeline.transition_ != nullptr) {
        framePipeline.transition_->display();
        transitionImage = framePipeline.transition_->getTexture().copyToImage();
    }
    engineState().setScale(renderScale);
    ludork::global::system_frame_pipeline_impl::initCanvas(size);
    if (transitionImage.has_value() && framePipeline.transition_ != nullptr) {
        const sf::Texture texture(*transitionImage);
        sf::Sprite sprite(texture);
        const sf::Vector2u sourceSize = texture.getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            framePipeline.transition_->clear(sf::Color::Transparent);
            framePipeline.transition_->draw(sprite, sf::BlendNone);
            framePipeline.transition_->display();
        }
    }
    if (framePipeline.transitionResource_ != nullptr &&
        framePipeline.transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize =
            framePipeline.transitionResource_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sf::Sprite maskSprite(*framePipeline.transitionResource_);
            maskSprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
            framePipeline.transitionMaskTexture_->draw(maskSprite,
                                                       sf::BlendNone);
            framePipeline.transitionMaskTexture_->display();
        }
    }
    ludork::global::system_display_impl::updateWindowViewport();
}

void applyConfiguredScale(float scale) {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.window_ == nullptr ||
        ludork::global::system_display_impl::isEmbeddedDisplay()) {
        return;
    }
    if (ludork::global::system_display_impl::isMobileDisplay()) {
        if (ludork::global::system_display_impl::isDisplayScaleConfigurable()) {
            ludork::global::native_display_host::requestDisplayScale(
                scale, ludork::global::system_display_impl::getGameSize());
        }
        return;
    }
    const bool fullscreen = scale == 0.0f;
    sf::Vector2u targetSize =
        fullscreen
            ? sf::VideoMode::getDesktopMode().size
            : ludork::global::system_display_impl::windowSizeForScale(scale);
    std::optional<sf::Vector2u> clientSize;
    if (fullscreen != display.desktopFullscreen_) {
        ludork::global::system_display_impl::recreateDesktopWindow(fullscreen,
                                                                   targetSize);
        if (!fullscreen) {
            clientSize = ludork::global::getWindowedClientSize(
                display.window_->getNativeHandle());
        }
    } else if (!fullscreen) {
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
        const std::optional<ludork::global::WindowedFramePlacement> placement =
            ludork::global::getWindowedFramePlacement(
                display.window_->getNativeHandle());
        ludork::global::system_display_impl::replaceWindowedDesktopWindow(
            targetSize, placement.has_value() ? &*placement : nullptr);
#else
        display.window_->setSize(targetSize);
#endif
        clientSize = ludork::global::getWindowedClientSize(
            display.window_->getNativeHandle());
    }
    display.observedWindowSize_ = display.window_->getSize();
    display.observedWindowClientSize_ = clientSize;
    display.pendingResizeScale_.reset();
    ludork::global::system_display_impl::rebuildDisplayTargets(
        ludork::global::system_display_impl::windowFitScale(
            clientSize.value_or(display.observedWindowSize_)));
}

void observeWindowResize() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.window_ == nullptr) {
        return;
    }
    const sf::Vector2u size = display.window_->getSize();
    const auto now = std::chrono::steady_clock::now();
    if (size != display.observedWindowSize_) {
        const std::optional<sf::Vector2u> clientSize =
            display.desktopFullscreen_
                ? std::nullopt
                : ludork::global::getWindowedClientSize(
                      display.window_->getNativeHandle());
        const bool clientSizeChanged =
            !clientSize.has_value() ||
            clientSize != display.observedWindowClientSize_;
        display.observedWindowSize_ = size;
        display.observedWindowClientSize_ = clientSize;
        if (clientSizeChanged) {
            display.pendingResizeScale_ =
                ludork::global::system_display_impl::windowFitScale(
                    clientSize.value_or(display.observedWindowSize_));
            display.lastResizeTime_ = now;
        }
        ludork::global::system_display_impl::updateWindowViewport();
    }
    if (!display.pendingResizeScale_.has_value() ||
        now - display.lastResizeTime_ < std::chrono::milliseconds(150)) {
        return;
    }
    float scale = *display.pendingResizeScale_;
    display.pendingResizeScale_.reset();
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!ludork::global::system_display_impl::isEmbeddedDisplay() &&
        !display.desktopFullscreen_) {
        const std::optional<sf::Vector2u> clientSize =
            ludork::global::getWindowedClientSize(
                display.window_->getNativeHandle());
        if (clientSize.has_value()) {
            const std::optional<ludork::global::WindowedFramePlacement>
                placement = ludork::global::getWindowedFramePlacement(
                    display.window_->getNativeHandle());
            ludork::global::system_display_impl::replaceWindowedDesktopWindow(
                *clientSize, placement.has_value() ? &*placement : nullptr);
            display.observedWindowSize_ = display.window_->getSize();
            const std::optional<sf::Vector2u> replacedClientSize =
                ludork::global::getWindowedClientSize(
                    display.window_->getNativeHandle());
            display.observedWindowClientSize_ = replacedClientSize;
            scale = ludork::global::system_display_impl::windowFitScale(
                replacedClientSize.value_or(*clientSize));
        }
    }
#endif
    ludork::global::system_display_impl::rebuildDisplayTargets(scale);
}

void applyPendingDisplayChanges() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.pendingConfiguredScale_.has_value()) {
        const float scale = *display.pendingConfiguredScale_;
        display.pendingConfiguredScale_.reset();
        ludork::global::system_display_impl::applyConfiguredScale(scale);
    }
    if (display.pendingRenderTargetRebuild_) {
        display.pendingRenderTargetRebuild_ = false;
        ludork::global::system_display_impl::rebuildDisplayTargets(
            display.surfaceFitScale_);
    }
    ludork::global::system_display_impl::observeWindowResize();
}

void setInputMethodDisabled(bool disabled) {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    display.inputMethodDisabled_ = disabled;
    if (display.window_ == nullptr ||
        ludork::global::runtimeLaunchOptions().windowMode ==
            ludork::global::RuntimeWindowMode::Embedded) {
        return;
    }
    ludork::global::setNativeInputMethodDisabled(
        display.window_->getNativeHandle(), disabled);
}

void clearCanvas() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.window_ != nullptr) {
        display.window_->clear(
            ludork::global::system_display_impl::isEmbeddedDisplay()
                ? sf::Color::Transparent
                : sf::Color::Black);
    }
    if (display.canvas_ != nullptr) {
        display.canvas_->clear(sf::Color::Transparent);
    }
}

void setWindowMapView(const sf::IntRect& rect) {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u gameSize =
        ludork::global::system_display_impl::getGameSize();
    const sf::Vector2f gameSizeFloat{static_cast<float>(gameSize.x),
                                     static_cast<float>(gameSize.y)};
    const sf::Vector2f position{static_cast<float>(rect.position.x),
                                static_cast<float>(rect.position.y)};
    const sf::Vector2f size{static_cast<float>(rect.size.x),
                            static_cast<float>(rect.size.y)};
    sf::View view(size / 2.0f, size);
    view.setViewport(sf::FloatRect(position.componentWiseDiv(gameSizeFloat),
                                   size.componentWiseDiv(gameSizeFloat)));
    display.canvas_->setView(view);
    display.canvasDefaultViewActive_ = false;
}

void setWindowDefaultView() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    if (display.canvas_ != nullptr) {
        display.canvas_->setView(display.canvas_->getDefaultView());
        display.canvasDefaultViewActive_ = true;
    }
}

sf::RenderTexture* getCanvas() {
    ludork::global::system_impl::DisplayImpl& display =
        ludork::global::system_impl::impl().display;
    return display.canvas_.get();
}

}  // namespace ludork::global::system_display_impl
