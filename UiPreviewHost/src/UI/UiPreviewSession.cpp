#include "UI/UiPreviewSession.hpp"

#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "UI/UiPreviewDrawing.hpp"
#include "UI/UiPreviewInstantiation.hpp"

#include <EngineState.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <UI/UiAssetRuntime.hpp>
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

RuntimeData UiPreviewSession::render(const RuntimeData::Map& request,
                                     FrameFiles& frameFiles) {
    const std::int64_t generation =
        ludork::runtime::value_reader::requireInteger(
            ludork::runtime::value_reader::requireValue(request, "generation",
                                                        "Render request"),
            "Render request.generation");
    const std::string& assetKey = ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(request, "assetKey",
                                                    "Render request"),
        "Render request.assetKey");
    const RuntimeData& asset = ludork::runtime::value_reader::requireValue(
        request, "asset", "Render request");
    const RuntimeData::Map& assetMap =
        ludork::runtime::value_reader::requireMap(asset,
                                                  "Render request.asset");
    const RuntimeData::Map& dependencies =
        ludork::runtime::value_reader::requireMap(
            ludork::runtime::value_reader::requireValue(request, "dependencies",
                                                        "Render request"),
            "Render request.dependencies");
    const sf::Vector2u design = designSize(assetMap);
    const double requestedScale = ludork::runtime::value_reader::requireNumber(
        ludork::runtime::value_reader::requireValue(request, "renderScale",
                                                    "Render request"),
        "Render request.renderScale");
    const RenderTargetSpec targetSpec =
        renderTargetSpec(design, requestedScale);
    std::shared_ptr<UiAssetInstance> instance = instantiateUiPreview(
        assetKey, asset, dependencies, design, targetSpec.renderScale);
    if (const RuntimeData* animationName =
            ludork::runtime::value_reader::findValue(request,
                                                     "animationName")) {
        const std::string& name = ludork::runtime::value_reader::requireString(
            *animationName, "Render request.animationName");
        const RuntimeData& targetValue =
            ludork::runtime::value_reader::requireValue(
                request, "animationTarget", "Render request");
        std::optional<std::string> target;
        if (!targetValue.isNil()) {
            target = ludork::runtime::value_reader::requireString(
                targetValue, "Render request.animationTarget");
        }
        const float time = ludork::runtime::value_reader::requireFloat(
            ludork::runtime::value_reader::requireValue(
                request, "animationTime", "Render request"),
            "Render request.animationTime");
        if (!instance->sampleAnimation(name, target, time)) {
            throw std::invalid_argument("UI preview animation was not found: " +
                                        name);
        }
    }
    const std::vector<std::uint8_t> pixels =
        renderFrame(instance, targetSpec.size);
    const std::filesystem::path& framePath = frameFiles.write(pixels);
    instance_ = std::move(instance);
    generation_ = generation;
    designSize_ = design;
    renderSize_ = targetSpec.size;
    renderScale_ = targetSpec.renderScale;
    return RuntimeData(object({
        {"type", RuntimeData("frame")},
        {"generation", RuntimeData(generation)},
        {"designWidth", RuntimeData(static_cast<std::int64_t>(design.x))},
        {"designHeight", RuntimeData(static_cast<std::int64_t>(design.y))},
        {"width", RuntimeData(static_cast<std::int64_t>(renderSize_.x))},
        {"height", RuntimeData(static_cast<std::int64_t>(renderSize_.y))},
        {"stride", RuntimeData(static_cast<std::int64_t>(renderSize_.x) * 4)},
        {"renderScale", number(renderScale_)},
        {"sharedMemory",
         RuntimeData(object({
             {"filePath", RuntimeData(ludork::standard::pathToUtf8(framePath))},
             {"offset", RuntimeData(std::int64_t{0})},
         }))},
        {"nodes",
         RuntimeData(nodeGeometry(instance_, renderSize_, renderScale_))},
    }));
}

RuntimeData UiPreviewSession::hitTest(const RuntimeData::Map& request) const {
    const std::int64_t generation =
        ludork::runtime::value_reader::requireInteger(
            ludork::runtime::value_reader::requireValue(request, "generation",
                                                        "Hit test request"),
            "Hit test request.generation");
    const sf::Vector2f logicalPoint{
        ludork::runtime::value_reader::requireFloat(
            ludork::runtime::value_reader::requireValue(request, "x",
                                                        "Hit test request"),
            "Hit test request.x"),
        ludork::runtime::value_reader::requireFloat(
            ludork::runtime::value_reader::requireValue(request, "y",
                                                        "Hit test request"),
            "Hit test request.y")};
    RuntimeData nodeName;
    if (instance_ != nullptr && generation == generation_) {
        engineState().setScale(renderScale_);
        const std::optional<std::string> hit = hitTestUiPreview(
            instance_, renderSize_, renderScale_, logicalPoint);
        if (hit.has_value()) {
            nodeName = RuntimeData(*hit);
        }
    }
    return RuntimeData(object({
        {"type", RuntimeData("hitTest")},
        {"generation", RuntimeData(generation)},
        {"nodeName", std::move(nodeName)},
    }));
}

}  // namespace ludork::preview_host
