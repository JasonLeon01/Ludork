#include "UI/UiPreviewSession.hpp"
#include <UI/UiAssetInstance.hpp>

#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "UI/UiPreviewDrawing.hpp"
#include "UI/UiPreviewInstantiation.hpp"

#include <EngineState.hpp>
#include <DataFile.hpp>
#include <Emitters/EmitterScheduler.hpp>
#include <Runtime/Json.hpp>
#include <ReadOnlyFileProvider.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <UI/EmitterView.hpp>
#include <UI/UiAssetRuntime.hpp>
#include <UI/UiEmitterTraversal.hpp>
#include <Utf8Path.hpp>

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Window/Context.hpp>

#include <chrono>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ludork::preview_host {
namespace {

void collectEmitterViews(const std::shared_ptr<ControlBase>& control,
                         std::vector<std::shared_ptr<EmitterView>>& emitters) {
    if (const std::shared_ptr<EmitterView> view =
            ludork::Cast<EmitterView>(control)) {
        emitters.push_back(view);
    }
    for (const std::shared_ptr<ControlBase>& child : control->getChildren()) {
        if (child != nullptr) {
            collectEmitterViews(child, emitters);
        }
    }
}

bool warmingEmitter(const std::shared_ptr<EmitterView>& view) {
    if (!view->getEmitter()->isPlaying() || !view->getEmitter()->isWarming()) {
        return false;
    }
    std::shared_ptr<ControlBase> control = view;
    while (control != nullptr) {
        if (!control->getVisible()) {
            return false;
        }
        control = control->getParent();
    }
    return true;
}

std::string particleResourceStamp(
    const std::vector<std::shared_ptr<EmitterView>>& views) {
    std::set<std::string> keys;
    for (const std::shared_ptr<EmitterView>& view : views) {
        if (!view->getParticle().empty()) {
            keys.insert(view->getParticle());
        }
    }
    RuntimeData::Array stamps;
    for (const std::string& key : keys) {
        const std::filesystem::path path =
            ludork::standard::resolveJsonDataPath(
                ludork::standard::pathFromUtf8("Data/Particles/" + key +
                                               ".json"));
        const ludork::standard::ReadOnlyFileStatus status =
            ludork::standard::readOnlyFileStatus(path);
        RuntimeData size;
        RuntimeData modified;
        if (status.handled) {
            size = RuntimeData(static_cast<std::int64_t>(status.size));
            modified = RuntimeData(status.modificationTime);
        } else {
            std::error_code error;
            const std::uintmax_t bytes =
                std::filesystem::file_size(path, error);
            if (!error) {
                size = RuntimeData(static_cast<std::int64_t>(bytes));
            }
            const std::filesystem::file_time_type time =
                std::filesystem::last_write_time(path, error);
            if (!error) {
                modified = RuntimeData(
                    static_cast<std::int64_t>(time.time_since_epoch().count()));
            }
        }
        stamps.emplace_back(object({
            {"path", RuntimeData(ludork::standard::pathToUtf8(path))},
            {"size", size},
            {"modified", modified},
        }));
    }
    return stringifyJSON(RuntimeData(std::move(stamps)));
}

}  // namespace

UiPreviewSession::UiPreviewSession() = default;

UiPreviewSession::~UiPreviewSession() {
    reset();
}

void UiPreviewSession::reset() noexcept {
    resetContent();
    target_.reset();
    context_.reset();
}

void UiPreviewSession::resetContent() noexcept {
    if (context_ != nullptr) {
        static_cast<void>(context_->setActive(true));
    }
    if (target_ != nullptr) {
        static_cast<void>(target_->setActive(true));
    }
    if (emitterScheduler_ != nullptr) {
        emitterScheduler_->shutdown();
    }
    emitterViews_.clear();
    instance_.reset();
    Emitter::collectGarbage();
    emitterScheduler_.reset();
    snapshot_.clear();
    particleResources_.clear();
    animationName_.clear();
    animationTarget_.reset();
    particleSteps_ = 0;
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
    std::string animationName;
    std::optional<std::string> animationTarget;
    float animationTime = 0.5f;
    if (const RuntimeData* animationNameValue =
            ludork::runtime::value_reader::findValue(request,
                                                     "animationName")) {
        const std::string& name = ludork::runtime::value_reader::requireString(
            *animationNameValue, "Render request.animationName");
        const RuntimeData& targetValue =
            ludork::runtime::value_reader::requireValue(
                request, "animationTarget", "Render request");
        std::optional<std::string> target;
        if (!targetValue.isNil()) {
            target = ludork::runtime::value_reader::requireString(
                targetValue, "Render request.animationTarget");
        }
        animationTime = ludork::runtime::value_reader::requireFloat(
            ludork::runtime::value_reader::requireValue(
                request, "animationTime", "Render request"),
            "Render request.animationTime");
        animationName = name;
        animationTarget = target;
    }
    double particleTime = animationTime;
    if (const RuntimeData* time =
            ludork::runtime::value_reader::findValue(request, "particleTime")) {
        particleTime = ludork::runtime::value_reader::requireNumber(
            *time, "Render request.particleTime");
    }
    if (particleTime < 0.0 || animationTime < 0.0f) {
        throw std::invalid_argument(
            "UI preview sample time must be nonnegative");
    }
    const std::string snapshot = stringifyJSON(RuntimeData(object({
        {"assetKey", RuntimeData(assetKey)},
        {"asset", asset},
        {"dependencies", RuntimeData(dependencies)},
        {"renderScale", number(targetSpec.renderScale)},
        {"animationName", RuntimeData(animationName)},
        {"animationTarget", animationTarget.has_value()
                                ? RuntimeData(*animationTarget)
                                : RuntimeData()},
    })));
    if (snapshot_ != snapshot ||
        particleResources_ != particleResourceStamp(emitterViews_)) {
        resetContent();
        if (context_ == nullptr) {
            context_ = std::make_unique<sf::Context>();
        }
        if (target_ == nullptr || target_->getSize() != targetSpec.size) {
            target_ = std::make_unique<sf::RenderTexture>(targetSpec.size);
        }
        if (!target_->setActive(true)) {
            throw std::runtime_error("Failed to activate UI preview target");
        }
        instance_ = instantiateUiPreview(assetKey, asset, dependencies, design,
                                         targetSpec.renderScale);
        collectEmitterViews(instance_->getRoot(), emitterViews_);
        particleResources_ = particleResourceStamp(emitterViews_);
        emitterScheduler_ = std::make_unique<EmitterScheduler>();
        animationName_ = animationName;
        animationTarget_ = animationTarget;
        snapshot_ = snapshot;
    }
    engineState().setScale(targetSpec.renderScale);
    if (!context_->setActive(true) || !target_->setActive(true)) {
        throw std::runtime_error("Failed to activate UI preview context");
    }
    const bool particleSeeking = sampleParticles(particleTime);
    if (!animationName_.empty()) {
        const float time = emitterViews_.empty()
                               ? animationTime
                               : static_cast<float>(particleSteps_) / 240.0f;
        if (!instance_->sampleAnimation(animationName_, animationTarget_,
                                        time)) {
            throw std::invalid_argument("UI preview animation was not found: " +
                                        animationName_);
        }
    }
    emitterScheduler_->beginFrame();
    ludork::engine::collectUiEmitters(instance_->getRoot(), *emitterScheduler_);
    const std::vector<std::uint8_t> pixels = renderFrame(instance_, *target_);
    const std::filesystem::path& framePath = frameFiles.write(pixels);
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
        {"particleSeeking", RuntimeData(particleSeeking)},
        {"particleTime",
         RuntimeData(static_cast<double>(particleSteps_) / 240.0)},
        {"sharedMemory",
         RuntimeData(object({
             {"filePath", RuntimeData(ludork::standard::pathToUtf8(framePath))},
             {"offset", RuntimeData(std::int64_t{0})},
         }))},
        {"nodes",
         RuntimeData(nodeGeometry(instance_, renderSize_, renderScale_))},
    }));
}

bool UiPreviewSession::sampleParticles(double time) {
    if (emitterViews_.empty()) {
        return false;
    }
    const double count = std::floor(time * 240.0 + 0.000001);
    if (count >=
        static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("UI particle sample time is too large");
    }
    const std::int64_t targetSteps = static_cast<std::int64_t>(count);
    if (targetSteps < particleSteps_) {
        for (const std::shared_ptr<EmitterView>& view : emitterViews_) {
            view->getEmitter()->restart();
            if (!view->getAutoPlay()) {
                view->getEmitter()->pause();
            }
        }
        particleSteps_ = 0;
    }
    const std::chrono::steady_clock::time_point begin =
        std::chrono::steady_clock::now();
    bool warming =
        std::any_of(emitterViews_.begin(), emitterViews_.end(), warmingEmitter);
    for (int step = 0; step < 120 && (warming || particleSteps_ < targetSteps);
         ++step) {
        if (!animationName_.empty() &&
            !instance_->sampleAnimation(
                animationName_, animationTarget_,
                static_cast<float>(particleSteps_) / 240.0f)) {
            throw std::invalid_argument("UI preview animation was not found: " +
                                        animationName_);
        }
        emitterScheduler_->beginFrame();
        ludork::engine::collectUiEmitters(instance_->getRoot(),
                                          *emitterScheduler_);
        emitterScheduler_->advance(warming ? 0.0f : 1.0f / 240.0f);
        if (!warming) {
            ++particleSteps_;
        }
        warming = std::any_of(emitterViews_.begin(), emitterViews_.end(),
                              warmingEmitter);
        if (std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin)
                .count() >= 12.0) {
            break;
        }
    }
    return warming || particleSteps_ < targetSteps;
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
