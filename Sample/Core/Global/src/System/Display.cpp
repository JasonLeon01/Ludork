#include <System.hpp>

#include "SystemRuntime.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"

#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>
#include <Manager/AssetPath.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>
#include <Runtime/EngineState.hpp>
#include <SystemConfigBase.hpp>
#include <Utils/Inner.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>

namespace {

bool viewsEqual(const sf::View& left, const sf::View& right) {
    return left.getCenter() == right.getCenter() &&
           left.getSize() == right.getSize() &&
           left.getRotation() == right.getRotation() &&
           left.getViewport() == right.getViewport() &&
           left.getScissor() == right.getScissor();
}

}  // namespace

void System::initializeDisplay(const std::string& title,
                               const sf::Vector2u& gameSize,
                               const std::string& iconPath,
                               const std::string& cursorPath) {
    if (gameSize.x == 0 || gameSize.y == 0) {
        throw std::invalid_argument("Game size must be non-zero");
    }
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
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
        static_cast<unsigned int>(getAntiAliasingLevel());
#if defined(SFML_SYSTEM_IOS)
    display.windowContextSettings_.majorVersion = 3;
    display.windowContextSettings_.minorVersion = 0;
#endif
    std::shared_ptr<sf::RenderWindow> window;

    setGameSize(gameSize);
    setDebugMode(launchOptions.editor);
    inputService().setUseInjectedMouseOnly(false);

    if (isEmbeddedDisplay()) {
#if defined(_WIN32)
        if (!launchOptions.hostWindowHandle.has_value()) {
            throw std::invalid_argument("Embedded window handle is required");
        }
        window = std::make_shared<sf::RenderWindow>(
            reinterpret_cast<sf::WindowHandle>(
                launchOptions.hostWindowHandle.value()),
            display.windowContextSettings_);
        engineState().setScale(windowFitScale(window->getSize()));
        inputService().setUseInjectedMouseOnly(true);
#else
        throw std::runtime_error(
            "Embedded window mode is only supported on Windows");
#endif
    } else if (isMobileDisplay()) {
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode::getDesktopMode(), title, sf::Style::Default,
            sf::State::Fullscreen, display.windowContextSettings_);
        engineState().setScale(windowFitScale(window->getSize()));
    } else {
        const float configuredScale = getConfiguredScale();
        display.desktopFullscreen_ = configuredScale == 0.0f;
        const sf::Vector2u windowSize =
            display.desktopFullscreen_ ? sf::VideoMode::getDesktopMode().size
                                       : renderSizeForScale(configuredScale);
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode(windowSize), title,
            display.desktopFullscreen_ ? sf::Style::None : sf::Style::Default,
            sf::State::Windowed, display.windowContextSettings_);
        const std::optional<sf::Vector2u> clientSize =
            display.desktopFullscreen_ ? std::nullopt
                                       : ludork::global::getWindowedClientSize(
                                             window->getNativeHandle());
        engineState().setScale(
            windowFitScale(clientSize.value_or(window->getSize())));
    }

#if defined(SFML_SYSTEM_IOS)
    if (window->getSettings().majorVersion < 3) {
        throw std::runtime_error(
            "iOS requires an OpenGL ES 3.0 context, but OpenGL ES " +
            std::to_string(window->getSettings().majorVersion) + "." +
            std::to_string(window->getSettings().minorVersion) +
            " was created");
    }
#endif
    initWindow(window);
    if (shadersAvailable()) {
        framePipeline.transitionShader_ = ShaderManager::load(
            "Global/Transition.frag", sf::Shader::Type::Fragment);
    } else {
        framePipeline.transitionShader_.reset();
        warnOnce("System.transitionShader",
                 "Shaders are unavailable; skipped loading transition shader");
    }
    setInputMethodDisabled(true);
    inputService().initializeNativePolling();
    initCanvas(renderSizeForScale(getScale()));
    display.observedWindowSize_ = display.window_->getSize();
    display.observedWindowClientSize_ =
        display.desktopFullscreen_ ? std::nullopt
                                   : ludork::global::getWindowedClientSize(
                                         display.window_->getNativeHandle());
    updateWindowViewport();
}

void System::initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    if (window == nullptr) {
        throw std::invalid_argument("System window cannot be nil");
    }
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    {
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        display.window_ = window;
    }
    applyWindowPresentationSettings();
}

std::shared_ptr<sf::RenderWindow> System::getWindow() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    return display.window_;
}

bool System::isEmbeddedDisplay() {
    return ludork::global::runtimeLaunchOptions().windowMode ==
           ludork::global::RuntimeWindowMode::Embedded;
}

bool System::isMobileDisplay() {
#if defined(LUDORK_MOBILE)
    return true;
#else
    return false;
#endif
}

float System::windowFitScale(const sf::Vector2u& size) {
    const sf::Vector2u gameSize = getGameSize();
    const float scale =
        std::min(static_cast<float>(size.x) / static_cast<float>(gameSize.x),
                 static_cast<float>(size.y) / static_cast<float>(gameSize.y));
    return std::max(0.01f, scale);
}

sf::Vector2u System::renderSizeForScale(float scale) {
    const sf::Vector2u gameSize = getGameSize();
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

void System::applyWindowPresentationSettings() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ == nullptr) {
        return;
    }
    display.window_->setFramerateLimit(
        static_cast<unsigned int>(std::max(0, getFrameRate())));
    display.window_->setVerticalSyncEnabled(getVerticalSync());
    display.window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                               : sf::Color::Black);
    if (!isMobileDisplay() && !display.windowIconPath_.empty()) {
        display.window_->setIcon(sf::Image(display.windowIconPath_));
    }
    display.cursor_.reset();
    if (isMobileDisplay() || display.windowCursorPath_.empty()) {
        return;
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(display.windowCursorPath_, error)) {
        return;
    }
    try {
        const sf::Image cursorImage(display.windowCursorPath_);
        display.cursor_ = std::make_unique<sf::Cursor>(
            cursorImage.getPixelsPtr(), cursorImage.getSize(), sf::Vector2u{});
        display.window_->setMouseCursor(*display.cursor_);
    } catch (const std::exception& exception) {
        std::cerr << "Failed to create cursor from "
                  << display.windowCursorPath_ << ": " << exception.what()
                  << '\n';
    }
}

void System::recreateDesktopWindow(bool fullscreen, const sf::Vector2u& size) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    if (display.window_ == nullptr || isEmbeddedDisplay() ||
        isMobileDisplay()) {
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
    applyWindowPresentationSettings();
    setInputMethodDisabled(display.inputMethodDisabled_);
    inputService().onWindowRecreated(*display.window_);
    inputService().initializeNativePolling();
}

void System::replaceWindowedDesktopWindow(
    const sf::Vector2u& size,
    const ludork::global::WindowedFramePlacement* placement) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    const std::shared_ptr<sf::RenderWindow> previousWindow = getWindow();
    if (previousWindow == nullptr || isEmbeddedDisplay() || isMobileDisplay()) {
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
        applyWindowPresentationSettings();
        setInputMethodDisabled(display.inputMethodDisabled_);
        inputService().onWindowRecreated(*display.window_);
        inputService().initializeNativePolling();
    }
}

void System::updateWindowViewport() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
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

void System::rebuildDisplayTargets(float scale) {
    const float normalizedScale = std::max(0.01f, scale);
    const sf::Vector2u size = renderSizeForScale(normalizedScale);
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    if (display.canvas_ != nullptr && display.canvas_->getSize() == size &&
        engineState().getScale() == normalizedScale) {
        updateWindowViewport();
        return;
    }
    std::optional<sf::Image> transitionImage;
    if (framePipeline.transition_ != nullptr) {
        framePipeline.transition_->display();
        transitionImage = framePipeline.transition_->getTexture().copyToImage();
    }
    engineState().setScale(normalizedScale);
    initCanvas(size);
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
    updateWindowViewport();
}

void System::applyConfiguredScale(float scale) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ == nullptr || isEmbeddedDisplay() ||
        isMobileDisplay()) {
        return;
    }
    const bool fullscreen = scale == 0.0f;
    sf::Vector2u targetSize = fullscreen ? sf::VideoMode::getDesktopMode().size
                                         : renderSizeForScale(scale);
    std::optional<sf::Vector2u> clientSize;
    if (fullscreen != display.desktopFullscreen_) {
        recreateDesktopWindow(fullscreen, targetSize);
        if (!fullscreen) {
            clientSize = ludork::global::getWindowedClientSize(
                display.window_->getNativeHandle());
        }
    } else if (!fullscreen) {
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
        const std::optional<ludork::global::WindowedFramePlacement> placement =
            ludork::global::getWindowedFramePlacement(
                display.window_->getNativeHandle());
        replaceWindowedDesktopWindow(
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
    rebuildDisplayTargets(
        windowFitScale(clientSize.value_or(display.observedWindowSize_)));
}

void System::observeWindowResize() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
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
            display.pendingResizeScale_ = windowFitScale(
                clientSize.value_or(display.observedWindowSize_));
            display.lastResizeTime_ = now;
        }
        updateWindowViewport();
    }
    if (!display.pendingResizeScale_.has_value() ||
        now - display.lastResizeTime_ < std::chrono::milliseconds(150)) {
        return;
    }
    float scale = *display.pendingResizeScale_;
    display.pendingResizeScale_.reset();
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !display.desktopFullscreen_) {
        const std::optional<sf::Vector2u> clientSize =
            ludork::global::getWindowedClientSize(
                display.window_->getNativeHandle());
        if (clientSize.has_value()) {
            const std::optional<ludork::global::WindowedFramePlacement>
                placement = ludork::global::getWindowedFramePlacement(
                    display.window_->getNativeHandle());
            replaceWindowedDesktopWindow(
                *clientSize, placement.has_value() ? &*placement : nullptr);
            display.observedWindowSize_ = display.window_->getSize();
            const std::optional<sf::Vector2u> replacedClientSize =
                ludork::global::getWindowedClientSize(
                    display.window_->getNativeHandle());
            display.observedWindowClientSize_ = replacedClientSize;
            scale = windowFitScale(replacedClientSize.value_or(*clientSize));
        }
    }
#endif
    rebuildDisplayTargets(scale);
}

void System::applyPendingDisplayChanges() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.pendingConfiguredScale_.has_value()) {
        const float scale = *display.pendingConfiguredScale_;
        display.pendingConfiguredScale_.reset();
        applyConfiguredScale(scale);
        return;
    }
    observeWindowResize();
}

void System::setInputMethodDisabled(bool disabled) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    display.inputMethodDisabled_ = disabled;
    if (display.window_ == nullptr ||
        ludork::global::runtimeLaunchOptions().windowMode ==
            ludork::global::RuntimeWindowMode::Embedded) {
        return;
    }
    ludork::global::setNativeInputMethodDisabled(
        display.window_->getNativeHandle(), disabled);
}

void System::initCanvas(const sf::Vector2u& size) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    std::optional<sf::View> preservedView;
    if (display.canvas_ == nullptr) {
        display.canvas_ = std::make_unique<sf::RenderTexture>(size);
    } else {
        const sf::View currentView = display.canvas_->getView();
        if (!display.canvasDefaultViewActive_ ||
            !viewsEqual(currentView, display.canvas_->getDefaultView())) {
            preservedView = currentView;
        }
        if (display.canvas_->getSize() != size &&
            !display.canvas_->resize(size)) {
            throw std::runtime_error("Failed to resize the System canvas");
        }
    }
    display.canvas_->setView(display.canvas_->getDefaultView());
    display.canvas_->clear(sf::Color::Transparent);
    display.canvas_->setView(
        preservedView.value_or(display.canvas_->getDefaultView()));
    if (display.canvasSprite_.has_value()) {
        display.canvasSprite_->setTexture(display.canvas_->getTexture(), true);
    } else {
        display.canvasSprite_.emplace(display.canvas_->getTexture());
    }
    framePipeline.transition_ = std::make_unique<sf::RenderTexture>(size);
    framePipeline.transition_->clear(sf::Color::Transparent);
    framePipeline.transition_->display();
    framePipeline.transitionTempTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionOutputTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionOutputTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionOutputTexture_->display();
    framePipeline.transitionMaskTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionMaskTexture_->display();
    framePipeline.transitionSprite_.emplace(
        framePipeline.transitionTempTexture_->getTexture());
    framePipeline.transitionOutputSprite_.emplace(
        framePipeline.transitionOutputTexture_->getTexture());
    framePipeline.toneBuffer_.reset();
    framePipeline.toneBufferSprite_.reset();
    applyGraphicsShadersLength();
}

void System::clearCanvas() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ != nullptr) {
        display.window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                                   : sf::Color::Black);
    }
    if (display.canvas_ != nullptr) {
        display.canvas_->clear(sf::Color::Transparent);
    }
}

void System::setWindowMapView(sf::Vector2f offset) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u gameSize = getGameSize();
    const sf::Vector2f size{static_cast<float>(gameSize.x),
                            static_cast<float>(gameSize.y)};
    const sf::Vector2f center = size / 2.0f - offset;
    display.canvas_->setView(sf::View(center, size));
    display.canvasDefaultViewActive_ = false;
}

void System::setWindowDefaultView() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.canvas_ != nullptr) {
        display.canvas_->setView(display.canvas_->getDefaultView());
        display.canvasDefaultViewActive_ = true;
    }
}

sf::RenderTexture* System::getCanvas() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    return display.canvas_.get();
}
