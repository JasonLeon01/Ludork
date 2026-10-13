#include "SubtitlePreviewSessionImpl.hpp"

#include <EngineState.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Utf8Path.hpp>
#include <VideoAudio.hpp>

#include "Protocol/FrameFiles.hpp"
#include "Protocol/PreviewProtocol.hpp"
#include "Rendering/PixelConversion.hpp"
#include "Rendering/PreviewResources.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ludork::preview_host {
namespace {

const RuntimeData& field(const RuntimeData::Map& request,
                         const std::string& name) {
    return ludork::runtime::value_reader::requireValue(request, name,
                                                       "Subtitle request");
}

RuntimeData status(const std::string& type, const std::string& sessionId,
                   std::int64_t generation) {
    return RuntimeData(object({{"type", RuntimeData(type)},
                               {"sessionId", RuntimeData(sessionId)},
                               {"generation", RuntimeData(generation)}}));
}

}  // namespace

void SubtitlePreviewSession::Impl::Session::loadVideo(const std::string& path) {
    if (path.empty()) {
        return;
    }
#if LUDORK_HAS_FFMPEG
    decoder = std::make_unique<ludork::video::VideoDecoder>(path);
    videoDuration = decoder->duration();
    const ludork::video::AudioData audio = ludork::video::extractAudio(path);
    if (!audio.samples.empty()) {
        soundBuffer.emplace();
        if (!soundBuffer->loadFromSamples(
                audio.samples.data(), audio.samples.size(), audio.channelCount,
                audio.sampleRate, audio.channelMap)) {
            throw std::runtime_error("Failed to load preview video audio");
        }
        sound.emplace(*soundBuffer);
        sound->setSpatializationEnabled(false);
    }
    updateVideo(0, false);
#else
    throw std::invalid_argument(
        "The current project does not support FFmpeg video playback");
#endif
}

void SubtitlePreviewSession::Impl::Session::configureTarget(sf::Vector2u size) {
    if (!context.setActive(true)) {
        throw std::runtime_error("Failed to activate subtitle preview context");
    }
    const unsigned int maximum = sf::Texture::getMaximumSize();
    if (size.x == 0 || size.y == 0 || size.x > maximum || size.y > maximum) {
        throw std::invalid_argument(
            "Subtitle preview size must fit the GPU texture limit");
    }
    if (!target || target->getSize() != size) {
        target = std::make_unique<sf::RenderTexture>(size);
    }
    const float scale = std::min(static_cast<float>(size.x) / logicalSize.x,
                                 static_cast<float>(size.y) / logicalSize.y);
    sf::View view(sf::FloatRect({0, 0}, logicalSize * scale));
    const sf::Vector2f fraction{
        logicalSize.x * scale / static_cast<float>(size.x),
        logicalSize.y * scale / static_cast<float>(size.y)};
    view.setViewport(
        sf::FloatRect((sf::Vector2f{1, 1} - fraction) / 2.0f, fraction));
    target->setView(view);
    engineState().setScale(scale);
#if LUDORK_HAS_FFMPEG
    if (sprite && texture) {
        const sf::Vector2u frameSize = texture->getSize();
        const float videoScale =
            std::min(view.getSize().x / static_cast<float>(frameSize.x),
                     view.getSize().y / static_cast<float>(frameSize.y));
        sprite->setScale({videoScale, videoScale});
        sprite->setPosition(view.getCenter());
    }
#endif
}

double SubtitlePreviewSession::Impl::Session::duration() const {
    return std::max(videoDuration, track.duration());
}

double SubtitlePreviewSession::Impl::Session::currentTime() const {
    return std::min(
        duration(),
        position + (playing ? clock.getElapsedTime().asSeconds() : 0.0));
}

void SubtitlePreviewSession::Impl::Session::pause() {
    position = currentTime();
    playing = false;
#if LUDORK_HAS_FFMPEG
    if (sound) {
        sound->pause();
    }
#endif
}

void SubtitlePreviewSession::Impl::Session::play() {
    if (playing || duration() <= 0) {
        return;
    }
    if (position >= duration()) {
        seek(0);
    }
    clock.restart();
    playing = true;
#if LUDORK_HAS_FFMPEG
    if (sound && position < soundBuffer->getDuration().asSeconds()) {
        sound->setVolume(mute ? 0.0f : 100.0f);
        sound->setPlayingOffset(sf::seconds(static_cast<float>(position)));
        sound->play();
    }
#endif
}

void SubtitlePreviewSession::Impl::Session::seek(double time) {
    const bool resume = playing;
    pause();
    position = std::clamp(time, 0.0, duration());
    updateVideo(position, true);
#if LUDORK_HAS_FFMPEG
    if (sound) {
        sound->setPlayingOffset(sf::seconds(static_cast<float>(position)));
    }
#endif
    if (resume && position < duration()) {
        play();
    }
}

void SubtitlePreviewSession::Impl::Session::updateVideo(double time,
                                                        bool seek) {
#if LUDORK_HAS_FFMPEG
    if (!decoder) {
        return;
    }
    if (videoDuration > 0 && time >= videoDuration) {
        sprite.reset();
        return;
    }
    bool changed = false;
    if (seek) {
        if (!decoder->seek(time)) {
            sprite.reset();
            return;
        }
        changed = true;
    } else {
        while (decoder->time() < time) {
            if (!decoder->readFrame()) {
                sprite.reset();
                return;
            }
            changed = true;
        }
    }
    if (!changed) {
        return;
    }
    const sf::Vector2u size{static_cast<unsigned int>(decoder->width()),
                            static_cast<unsigned int>(decoder->height())};
    if (!texture || texture->getSize() != size) {
        sprite.reset();
        texture.emplace(size);
    }
    texture->update(decoder->rgbaFrame().data());
    sprite.emplace(*texture);
    const sf::View& view = target->getView();
    const float scale = std::min(view.getSize().x / static_cast<float>(size.x),
                                 view.getSize().y / static_cast<float>(size.y));
    sprite->setScale({scale, scale});
    sprite->setOrigin(sf::Vector2f(size) / 2.0f);
    sprite->setPosition(view.getCenter());
#else
    static_cast<void>(time);
    static_cast<void>(seek);
#endif
}

RuntimeData SubtitlePreviewSession::Impl::Session::frame(
    const std::string& sessionId, std::int64_t generation,
    FrameFiles& frameFiles) {
    const double time = currentTime();
    if (playing && time >= duration()) {
        pause();
    }
    updateVideo(time, false);
    target->clear(sf::Color(24, 24, 24));
#if LUDORK_HAS_FFMPEG
    if (sound) {
        sound->setVolume(mute ? 0.0f : 100.0f);
    }
    if (sprite) {
        target->draw(*sprite);
    }
#endif
    renderer.draw(*target, track.linesAt(time, language));
    target->display();
    const sf::Vector2u size = target->getSize();
    const sf::Image image = target->getTexture().copyToImage();
    const auto& framePath =
        frameFiles.write(bgraFromPremultipliedRgba(image, size));
    return RuntimeData(
        object({{"type", RuntimeData("subtitleFrame")},
                {"sessionId", RuntimeData(sessionId)},
                {"generation", RuntimeData(generation)},
                {"time", RuntimeData(time)},
                {"duration", RuntimeData(duration())},
                {"videoDuration", RuntimeData(videoDuration)},
                {"playing", RuntimeData(playing)},
                {"width", RuntimeData(static_cast<std::int64_t>(size.x))},
                {"height", RuntimeData(static_cast<std::int64_t>(size.y))},
                {"stride", RuntimeData(static_cast<std::int64_t>(size.x) * 4)},
                {"sharedMemory",
                 RuntimeData(object(
                     {{"filePath",
                       RuntimeData(ludork::standard::pathToUtf8(framePath))},
                      {"offset", RuntimeData(std::int64_t{0})}}))}}));
}

RuntimeData SubtitlePreviewSession::Impl::render(
    const RuntimeData::Map& request, FrameFiles& frameFiles) {
    namespace reader = ludork::runtime::value_reader;
    const std::string& id = reader::requireString(field(request, "sessionId"),
                                                  "Subtitle sessionId");
    const std::int64_t generation = reader::requireInteger(
        field(request, "generation"), "Subtitle generation");
    const std::string& command =
        reader::requireString(field(request, "command"), "Subtitle command");
    if (id.empty() || generation < 0) {
        throw std::invalid_argument(
            "Subtitle sessionId must be nonempty and generation nonnegative");
    }
    const auto previous = generations.find(id);
    if (previous != generations.end() &&
        (generation < previous->second ||
         (!sessions.contains(id) && generation == previous->second &&
          command != "close"))) {
        return status("subtitleStale", id, generation);
    }
    if (command == "close") {
        sessions.erase(id);
        generations.insert_or_assign(id, generation);
        return status("subtitleClosed", id, generation);
    }
    if (command != "load" && command != "render" && command != "play" &&
        command != "pause" && command != "seek" && command != "stop") {
        throw std::invalid_argument("Unknown subtitle preview command: " +
                                    command);
    }
    const sf::Vector2u size{
        reader::requireUnsigned(field(request, "width"), "Subtitle width"),
        reader::requireUnsigned(field(request, "height"), "Subtitle height")};
    if (command == "load") {
        auto loaded = std::make_unique<Session>();
        loaded->track =
            ludork::video::SubtitleTrack::parse(field(request, "asset"));
        loaded->logicalSize = previewGameSize();
        loaded->configureTarget(size);
        configureUiResources();
        const RuntimeData* path = reader::findValue(request, "videoPath");
        loaded->loadVideo(path == nullptr ? ""
                                          : reader::requireString(
                                                *path, "Subtitle videoPath"));
        sessions.insert_or_assign(id, std::move(loaded));
    } else if (!sessions.contains(id)) {
        throw std::invalid_argument("Subtitle preview session has not loaded");
    }
    Session& session = *sessions.at(id);
    session.configureTarget(size);
    if (command != "load") {
        if (const RuntimeData* asset = reader::findValue(request, "asset")) {
            session.track = ludork::video::SubtitleTrack::parse(*asset);
        }
    }
    if (const RuntimeData* language = reader::findValue(request, "language")) {
        session.language =
            reader::requireString(*language, "Subtitle language");
    }
    if (const RuntimeData* mute = reader::findValue(request, "mute")) {
        session.mute = reader::requireBool(*mute, "Subtitle mute");
    }
    if (command == "play") {
        session.play();
    } else if (command == "pause") {
        session.pause();
    } else if (command == "stop") {
        session.pause();
        session.seek(0);
    } else if (command == "seek") {
        const double time =
            reader::requireNumber(field(request, "time"), "Subtitle time");
        if (time < 0) {
            throw std::invalid_argument("Subtitle time must be nonnegative");
        }
        session.seek(time);
    }
    generations.insert_or_assign(id, generation);
    return session.frame(id, generation, frameFiles);
}

SubtitlePreviewSession::SubtitlePreviewSession()
    : impl_(std::make_unique<Impl>()) {}
SubtitlePreviewSession::~SubtitlePreviewSession() = default;

void SubtitlePreviewSession::reset() noexcept {
    impl_->sessions.clear();
    impl_->generations.clear();
}

RuntimeData SubtitlePreviewSession::render(const RuntimeData::Map& request,
                                           FrameFiles& frameFiles) {
    return impl_->render(request, frameFiles);
}

}  // namespace ludork::preview_host
