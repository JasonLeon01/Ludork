#include <Graphics.hpp>
#include "System/FramePipelineImpl.hpp"
#include "System/DisplayImpl.hpp"
#include "System/Config/GraphicsConfigImpl.hpp"
#include <EngineState.hpp>
#include <algorithm>
#include <utility>

float Graphics::getMaximumRenderScale() {
    return ludork::global::system_impl::GraphicsConfigImpl::
        getMaximumRenderScale();
}

void Graphics::setMaximumRenderScale(float value) {
    ludork::global::system_impl::GraphicsConfigImpl::setMaximumRenderScale(
        value);
}

void Graphics::saveMaximumRenderScale(float value) {
    ludork::global::system_impl::GraphicsConfigImpl::saveMaximumRenderScale(
        value);
}

float Graphics::getLightingRenderScale() {
    return ludork::global::system_impl::GraphicsConfigImpl::
        getLightingRenderScale();
}

void Graphics::setLightingRenderScale(float value) {
    ludork::global::system_impl::GraphicsConfigImpl::setLightingRenderScale(
        value);
}

void Graphics::saveLightingRenderScale(float value) {
    ludork::global::system_impl::GraphicsConfigImpl::saveLightingRenderScale(
        value);
}

void Graphics::initCanvas(const sf::Vector2u& size) {
    ludork::global::system_impl::framePipelineImpl().initCanvas(size);
}

void Graphics::clearCanvas() {
    ludork::global::system_impl::displayImpl().clearWindow();
    ludork::global::system_impl::framePipelineImpl().clearCanvas();
}

void Graphics::setWindowMapView(const sf::IntRect& rect) {
    ludork::global::system_impl::framePipelineImpl().setWindowMapView(rect);
}

void Graphics::setWindowDefaultView() {
    ludork::global::system_impl::framePipelineImpl().setWindowDefaultView();
}

sf::RenderTexture* Graphics::getCanvas() {
    return ludork::global::system_impl::framePipelineImpl().getCanvas();
}

void Graphics::draw(const sf::Drawable& drawable, sf::Shader* shader) {
    ludork::global::system_impl::framePipelineImpl().draw(drawable, shader);
}

void Graphics::composeFrame(float deltaTime) {
    const std::shared_ptr<sf::RenderWindow> window =
        ludork::global::system_impl::displayImpl().getWindow();
    ludork::global::system_impl::framePipelineImpl().composeFrame(deltaTime,
                                                                  window.get());
}

void Graphics::present() {
    ludork::global::system_impl::framePipelineImpl().present(
        ludork::global::system_impl::displayImpl());
}

void Graphics::completeFrame() {
    if (ludork::global::system_impl::framePipelineImpl().completeFrame(
            ludork::global::system_impl::displayImpl().getWindow() !=
            nullptr)) {
        applyPendingDisplayChanges();
    }
}

void Graphics::addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                                 std::optional<ShaderUniforms> uniforms) {
    ludork::global::system_impl::framePipelineImpl().addGraphicsShader(
        shader, std::move(uniforms));
}

void Graphics::removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader) {
    ludork::global::system_impl::framePipelineImpl().removeGraphicsShader(
        shader);
}

void Graphics::removeAllGraphicsShaders() {
    ludork::global::system_impl::framePipelineImpl().removeAllGraphicsShaders();
}

void Graphics::removeGraphicsShaderAt(int index) {
    ludork::global::system_impl::framePipelineImpl().removeGraphicsShaderAt(
        index);
}

void Graphics::rebuildDisplayTargets(float surfaceFitScale) {
    const float normalizedSurfaceFitScale = std::max(0.01f, surfaceFitScale);
    const float renderScale =
        ludork::global::system_impl::displayImpl().effectiveRenderScale(
            normalizedSurfaceFitScale);
    const sf::Vector2u size =
        ludork::global::system_impl::displayImpl().renderSizeForScale(
            renderScale);
    ludork::global::system_impl::displayImpl().setSurfaceFitScale(
        normalizedSurfaceFitScale);
    if (ludork::global::system_impl::framePipelineImpl().getCanvasSize() ==
            size &&
        engineState().getScale() == renderScale) {
        ludork::global::system_impl::displayImpl().updateWindowViewport(size);
        return;
    }
    ludork::global::system_impl::framePipelineImpl().rebuildTargets(
        size, renderScale);
    ludork::global::system_impl::displayImpl().updateWindowViewport(
        ludork::global::system_impl::framePipelineImpl().getCanvasSize());
}

void Graphics::applyPendingDisplayChanges() {
    if (const std::optional<float> configuredScale =
            ludork::global::system_impl::displayImpl().takeConfiguredScale();
        configuredScale.has_value()) {
        if (const std::optional<float> surfaceScale =
                ludork::global::system_impl::displayImpl().applyConfiguredScale(
                    *configuredScale);
            surfaceScale.has_value()) {
            rebuildDisplayTargets(*surfaceScale);
        }
    }
    if (ludork::global::system_impl::displayImpl().takeRenderTargetRebuild()) {
        rebuildDisplayTargets(
            ludork::global::system_impl::displayImpl().getSurfaceFitScale());
    }
    if (const std::optional<float> resizeScale =
            ludork::global::system_impl::displayImpl().observeWindowResize(
                ludork::global::system_impl::framePipelineImpl()
                    .getCanvasSize());
        resizeScale.has_value()) {
        rebuildDisplayTargets(*resizeScale);
    }
}

void Graphics::onConfigurationChanged(const std::string& key) {
    if (key == "maximumRenderScale" &&
        ludork::global::system_impl::displayImpl().getWindow() != nullptr) {
        ludork::global::system_impl::displayImpl().requestRenderTargetRebuild();
    }
}
