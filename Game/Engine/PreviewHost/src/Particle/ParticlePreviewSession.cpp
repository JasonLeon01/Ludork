#include "Particle/ParticlePreviewSessionImpl.hpp"

#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "Rendering/PixelConversion.hpp"

#include <Emitters/Emitter.hpp>
#include <Emitters/EmitterResource.hpp>
#include <Runtime/Graphics/EmitterStatistics.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Utf8Path.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::preview_host {
namespace {

const RuntimeData& field(const RuntimeData::Map& request,
                         const std::string& name) {
    return ludork::runtime::value_reader::requireValue(request, name,
                                                       "Particle request");
}

double positiveNumber(const RuntimeData::Map& request, const std::string& name,
                      double defaultValue) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(request, name);
    const double result = value == nullptr
                              ? defaultValue
                              : ludork::runtime::value_reader::requireNumber(
                                    *value, "Particle request." + name);
    if (result <= 0.0 || result > std::numeric_limits<float>::max()) {
        throw std::invalid_argument("Particle request." + name +
                                    " must be positive and within float range");
    }
    return result;
}

double nonnegativeNumber(const RuntimeData::Map& request,
                         const std::string& name) {
    const double result = ludork::runtime::value_reader::requireNumber(
        field(request, name), "Particle request." + name);
    if (result < 0.0 || result > std::numeric_limits<float>::max()) {
        throw std::invalid_argument(
            "Particle request." + name +
            " must be nonnegative and within float range");
    }
    return result;
}

double elapsedMilliseconds(std::chrono::steady_clock::time_point begin,
                           std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

RuntimeData statisticsData(
    const ludork::runtime::graphics::EmitterStatistics& statistics) {
    RuntimeData::Map result{
        {"capacity",
         RuntimeData(static_cast<std::int64_t>(statistics.capacity))},
        {"time", RuntimeData(statistics.time)}};
    const auto add = [&result](const char* name, const auto& value) {
        if (value.has_value()) {
            result.emplace(name, RuntimeData(*value));
        }
    };
    add("renderer", statistics.renderer);
    add("aliveCount", statistics.aliveCount);
    add("sampledTime", statistics.sampledTime);
    add("gpuSimulationMs", statistics.gpuSimulationMs);
    add("simulationSample", statistics.simulationSample);
    add("gpuDrawMs", statistics.gpuDrawMs);
    add("drawSample", statistics.drawSample);
    return RuntimeData(std::move(result));
}

RuntimeData statusResponse(const std::string& type,
                           const std::string& sessionId,
                           std::int64_t generation) {
    return RuntimeData(object({
        {"type", RuntimeData(type)},
        {"sessionId", RuntimeData(sessionId)},
        {"generation", RuntimeData(generation)},
    }));
}

}  // namespace

ParticlePreviewSession::Impl::Session::Session() = default;

ParticlePreviewSession::Impl::Session::~Session() noexcept {
    if (emitter != nullptr) {
        try {
            activate();
            emitter->shutdown();
            Emitter::collectGarbage();
        } catch (const std::exception& exception) {
            std::cerr << "Particle preview shutdown: " << exception.what()
                      << '\n';
        }
        emitter.reset();
    }
    target.reset();
}

void ParticlePreviewSession::Impl::Session::activate() {
    if (!context.setActive(true) ||
        (target != nullptr && !target->setActive(true))) {
        throw std::runtime_error("Failed to activate particle preview context");
    }
}

void ParticlePreviewSession::Impl::Session::configureTarget(
    const sf::Vector2u& size, float zoom) {
    activate();
    const unsigned int maximumSize = sf::Texture::getMaximumSize();
    if (size.x == 0 || size.y == 0 || size.x > maximumSize ||
        size.y > maximumSize) {
        throw std::invalid_argument(
            "Particle preview size must fit the GPU texture size limit");
    }
    if (target == nullptr || target->getSize() != size) {
        target = std::make_unique<sf::RenderTexture>(size);
    }
    activate();
    sf::View view;
    view.setCenter({0.0f, 0.0f});
    view.setSize(
        {static_cast<float>(size.x) / zoom, static_cast<float>(size.y) / zoom});
    target->setView(view);
}

void ParticlePreviewSession::Impl::Session::rewind() {
    emitter->restart();
    emitter->pause();
    seekTime.reset();
    seekSteps = 0;
    timeOffset = 0;
}

bool ParticlePreviewSession::Impl::Session::seek(double time) {
    const double stepCount = std::floor(time * simulationRate + 0.000001);
    if (stepCount >=
        static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument("Particle seek time is too large");
    }
    if (!seekTime.has_value() || *seekTime != time) {
        rewind();
        seekTime = time;
    }
    const std::int64_t targetSteps = static_cast<std::int64_t>(stepCount);
    const std::chrono::steady_clock::time_point begin =
        std::chrono::steady_clock::now();
    emitter->setSpeed(1.0f);
    emitter->resume();
    for (int count = 0;
         count < 120 && (emitter->isWarming() || seekSteps < targetSteps);
         ++count) {
        if (emitter->isWarming()) {
            emitter->tick(0.0f);
        } else if (!emitter->isPlaying()) {
            seekSteps = targetSteps;
            break;
        } else {
            emitter->tick(1.0f / static_cast<float>(simulationRate));
            ++seekSteps;
        }
        if (elapsedMilliseconds(begin, std::chrono::steady_clock::now()) >=
            12.0) {
            break;
        }
    }
    timeOffset =
        static_cast<double>(seekSteps) / simulationRate - emitter->getTime();
    const bool seeking = emitter->isWarming() || seekSteps < targetSteps;
    emitter->pause();
    return seeking;
}

RuntimeData ParticlePreviewSession::Impl::Session::frame(
    const std::string& sessionId, std::int64_t generation, bool seeking,
    FrameFiles& frameFiles) {
    target->clear(sf::Color::Transparent);
    emitter->draw(*target, sf::RenderStates::Default);
    target->display();
    const RuntimeData statistics = statisticsData(emitter->getStatistics());
    const std::chrono::steady_clock::time_point readbackBegin =
        std::chrono::steady_clock::now();
    const sf::Image image = target->getTexture().copyToImage();
    const std::chrono::steady_clock::time_point transferBegin =
        std::chrono::steady_clock::now();
    const sf::Vector2u size = target->getSize();
    const std::vector<std::uint8_t> pixels =
        bgraFromPremultipliedRgba(image, size);
    const std::filesystem::path& framePath = frameFiles.write(pixels);
    const std::chrono::steady_clock::time_point transferEnd =
        std::chrono::steady_clock::now();
    return RuntimeData(object({
        {"type", RuntimeData("particleFrame")},
        {"sessionId", RuntimeData(sessionId)},
        {"generation", RuntimeData(generation)},
        {"time", RuntimeData(timeOffset + emitter->getTime())},
        {"seeking", RuntimeData(seeking)},
        {"stats", statistics},
        {"previewReadbackMs",
         RuntimeData(elapsedMilliseconds(readbackBegin, transferBegin))},
        {"previewTransferMs",
         RuntimeData(elapsedMilliseconds(transferBegin, transferEnd))},
        {"width", RuntimeData(static_cast<std::int64_t>(size.x))},
        {"height", RuntimeData(static_cast<std::int64_t>(size.y))},
        {"stride", RuntimeData(static_cast<std::int64_t>(size.x) * 4)},
        {"sharedMemory",
         RuntimeData(object({
             {"filePath", RuntimeData(ludork::standard::pathToUtf8(framePath))},
             {"offset", RuntimeData(std::int64_t{0})},
         }))},
    }));
}

RuntimeData ParticlePreviewSession::Impl::render(
    const RuntimeData::Map& request, FrameFiles& frameFiles) {
    const std::string& sessionId = ludork::runtime::value_reader::requireString(
        field(request, "sessionId"), "Particle request.sessionId");
    const std::int64_t generation =
        ludork::runtime::value_reader::requireInteger(
            field(request, "generation"), "Particle request.generation");
    const std::string& command = ludork::runtime::value_reader::requireString(
        field(request, "command"), "Particle request.command");
    if (sessionId.empty() || generation < 0) {
        throw std::invalid_argument(
            "Particle sessionId must be nonempty and generation nonnegative");
    }
    const auto previousGeneration = generations.find(sessionId);
    const auto iterator = sessions.find(sessionId);
    if (previousGeneration != generations.end() &&
        (generation < previousGeneration->second ||
         (iterator == sessions.end() && command != "close" &&
          generation == previousGeneration->second))) {
        return statusResponse("particleStale", sessionId, generation);
    }
    if (command == "close") {
        sessions.erase(sessionId);
        generations.insert_or_assign(sessionId, generation);
        return statusResponse("particleClosed", sessionId, generation);
    }
    if (command != "load" && command != "render" && command != "advance" &&
        command != "seek" && command != "reset") {
        throw std::invalid_argument("Unknown particle preview command: " +
                                    command);
    }
    const sf::Vector2u size{
        ludork::runtime::value_reader::requireUnsigned(
            field(request, "width"), "Particle request.width"),
        ludork::runtime::value_reader::requireUnsigned(
            field(request, "height"), "Particle request.height")};
    const float zoom = static_cast<float>(positiveNumber(request, "zoom", 1.0));
    const float speed =
        static_cast<float>(positiveNumber(request, "speed", 1.0));
    if (command == "load") {
        ludork::runtime::value_reader::requireString(
            field(request, "assetKey"), "Particle request.assetKey");
        const RuntimeData& asset = field(request, "asset");
        const EmitterConfiguration configuration =
            ludork::engine::emitters::parseEmitterConfiguration(asset);
        std::unique_ptr<Session> loaded = std::make_unique<Session>();
        loaded->configureTarget(size, zoom);
        loaded->emitter = std::make_unique<Emitter>();
        loaded->emitter->setConfiguration(configuration);
        loaded->emitter->setProfiling(true);
        loaded->simulationRate = configuration.simulationRate;
        loaded->rewind();
        sessions.insert_or_assign(sessionId, std::move(loaded));
    } else if (iterator == sessions.end()) {
        throw std::invalid_argument("Particle preview session has not loaded");
    }
    Session& session = *sessions.at(sessionId);
    session.configureTarget(size, zoom);
    session.emitter->setSpeed(speed);
    bool seeking = false;
    if (command == "advance") {
        const float deltaTime =
            static_cast<float>(nonnegativeNumber(request, "deltaTime"));
        session.seekTime.reset();
        if (!session.emitter->isPlaying()) {
            session.emitter->resume();
        }
        session.emitter->tick(deltaTime);
    } else if (command == "seek") {
        seeking = session.seek(nonnegativeNumber(request, "time"));
    } else if (command == "reset") {
        session.rewind();
    }
    generations.insert_or_assign(sessionId, generation);
    return session.frame(sessionId, generation, seeking, frameFiles);
}

ParticlePreviewSession::ParticlePreviewSession()
    : impl_(std::make_unique<Impl>()) {}

ParticlePreviewSession::~ParticlePreviewSession() = default;

void ParticlePreviewSession::reset() noexcept {
    impl_->sessions.clear();
    impl_->generations.clear();
}

RuntimeData ParticlePreviewSession::render(const RuntimeData::Map& request,
                                           FrameFiles& frameFiles) {
    return impl_->render(request, frameFiles);
}

}  // namespace ludork::preview_host
