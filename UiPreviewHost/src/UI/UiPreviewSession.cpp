#include "UI/UiPreviewSession.hpp"

#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "UI/UiPreviewDrawing.hpp"
#include "UI/UiPreviewInstantiation.hpp"

#include <Runtime/EngineState.hpp>
#include <Utf8Path.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ludork::preview_host {

void UiPreviewSession::reset() noexcept {
    instance_.reset();
    generation_ = 0;
    designSize_ = {};
    renderSize_ = {};
    renderScale_ = 1.0f;
}

RuntimeValue UiPreviewSession::render(const RuntimeValue::Map& request,
                                      FrameFiles& frameFiles) {
    const std::int64_t generation =
        requireInteger(requireValue(request, "generation", "Render request"),
                       "Render request.generation");
    const std::string& assetKey =
        requireString(requireValue(request, "assetKey", "Render request"),
                      "Render request.assetKey");
    const RuntimeValue& asset =
        requireValue(request, "asset", "Render request");
    const RuntimeValue::Map& assetMap =
        requireMap(asset, "Render request.asset");
    const RuntimeValue::Map& dependencies =
        requireMap(requireValue(request, "dependencies", "Render request"),
                   "Render request.dependencies");
    const sf::Vector2u design = designSize(assetMap);
    const double requestedScale =
        requireNumber(requireValue(request, "renderScale", "Render request"),
                      "Render request.renderScale");
    const RenderTargetSpec targetSpec =
        renderTargetSpec(design, requestedScale);
    std::shared_ptr<UiAssetInstance> instance = instantiateUiPreview(
        assetKey, asset, dependencies, design, targetSpec.renderScale);
    const std::vector<std::uint8_t> pixels =
        renderFrame(instance, targetSpec.size);
    const std::filesystem::path& framePath = frameFiles.write(pixels);
    instance_ = std::move(instance);
    generation_ = generation;
    designSize_ = design;
    renderSize_ = targetSpec.size;
    renderScale_ = targetSpec.renderScale;
    return RuntimeValue(object({
        {"type", RuntimeValue("frame")},
        {"generation", RuntimeValue(generation)},
        {"designWidth", RuntimeValue(static_cast<std::int64_t>(design.x))},
        {"designHeight", RuntimeValue(static_cast<std::int64_t>(design.y))},
        {"width", RuntimeValue(static_cast<std::int64_t>(renderSize_.x))},
        {"height", RuntimeValue(static_cast<std::int64_t>(renderSize_.y))},
        {"stride", RuntimeValue(static_cast<std::int64_t>(renderSize_.x) * 4)},
        {"renderScale", number(renderScale_)},
        {"sharedMemory",
         RuntimeValue(object({
             {"filePath",
              RuntimeValue(ludork::standard::pathToUtf8(framePath))},
             {"offset", RuntimeValue(std::int64_t{0})},
         }))},
        {"nodes",
         RuntimeValue(nodeGeometry(instance_, renderSize_, renderScale_))},
    }));
}

RuntimeValue UiPreviewSession::hitTest(const RuntimeValue::Map& request) const {
    const std::int64_t generation =
        requireInteger(requireValue(request, "generation", "Hit test request"),
                       "Hit test request.generation");
    const sf::Vector2f logicalPoint{
        static_cast<float>(
            requireNumber(requireValue(request, "x", "Hit test request"),
                          "Hit test request.x")),
        static_cast<float>(
            requireNumber(requireValue(request, "y", "Hit test request"),
                          "Hit test request.y"))};
    RuntimeValue nodeName;
    if (instance_ != nullptr && generation == generation_) {
        engineState().setScale(renderScale_);
        const std::optional<std::string> hit = hitTestUiPreview(
            instance_, renderSize_, renderScale_, logicalPoint);
        if (hit.has_value()) {
            nodeName = RuntimeValue(*hit);
        }
    }
    return RuntimeValue(object({
        {"type", RuntimeValue("hitTest")},
        {"generation", RuntimeValue(generation)},
        {"nodeName", std::move(nodeName)},
    }));
}

}  // namespace ludork::preview_host
