#include "VideoPlayerImpl.hpp"

#if LUDORK_HAS_FFMPEG
#include <Input/InputService.hpp>
#include <Manager/TimeManager.hpp>
#include <System.hpp>

#include <SFML/Audio/SoundBuffer.hpp>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#else
#include <iostream>
#endif

namespace ludork::video {

#if LUDORK_HAS_FFMPEG
WindowFocusRestoreScope::WindowFocusRestoreScope(const sf::RenderWindow& window)
    : restore_(window.hasFocus()) {}

WindowFocusRestoreScope::~WindowFocusRestoreScope() {
    const std::shared_ptr<sf::RenderWindow> window = System::getWindow();
    if (restore_ && window != nullptr && window->isOpen()) {
        window->requestFocus();
    }
}

VideoPlayerImpl::VideoPlayerImpl(std::string path, bool mute, bool skipable)
    : path_(std::move(path)),
      mute_(mute),
      skipable_(skipable),
      decoder_(path_),
      audio_(extractAudio(path_)) {}

void VideoPlayerImpl::play() {
    std::shared_ptr<sf::RenderWindow> window = System::getWindow();
    if (window == nullptr) {
        throw std::runtime_error("Video playback requires an active window");
    }
    WindowFocusRestoreScope focusRestoreScope(*window);

    std::optional<sf::SoundBuffer> soundBuffer;
    std::optional<sf::Sound> sound;
    if (!audio_.samples.empty()) {
        soundBuffer.emplace();
        if (!soundBuffer->loadFromSamples(
                audio_.samples.data(), audio_.samples.size(),
                audio_.channelCount, audio_.sampleRate, audio_.channelMap)) {
            throw std::runtime_error("Failed to load decoded video audio");
        }
        sound.emplace(*soundBuffer);
        sound->setSpatializationEnabled(false);
        sound->setVolume(mute_ ? 0.0f : 100.0f);
        sound->play();
    }

    sf::Clock silentClock;
    silentClock.restart();
    while (System::isActive()) {
        window = System::getWindow();
        if (window == nullptr) {
            break;
        }
        inputService().update(*window);
        TimeManager::update();
        if (skipable_ && inputService().isActionTriggered(
                             inputService().getConfirmKeys(), true)) {
            break;
        }
        window->clear(sf::Color::Transparent);
        update(*window, sound, silentClock);
        if (sprite_.has_value()) {
            window->draw(*sprite_);
        }
        System::present();
        window.reset();
        System::completeFrame();
        if (finished_) {
            break;
        }
    }
    if (sound.has_value()) {
        sound->stop();
    }
}

void VideoPlayerImpl::update(sf::RenderWindow& window,
                             const std::optional<sf::Sound>& sound,
                             const sf::Clock& silentClock) {
    const float elapsed = sound.has_value()
                              ? sound->getPlayingOffset().asSeconds()
                              : silentClock.getElapsedTime().asSeconds();
    const int expectedFrame =
        static_cast<int>(elapsed * static_cast<float>(decoder_.fps()));
    if (sound.has_value() &&
        sound->getStatus() == sf::SoundSource::Status::Stopped &&
        decoder_.frameIndex() > 0) {
        finished_ = true;
        return;
    }
    while (targetFrameIndex_.has_value() &&
           (expectedFrame > *targetFrameIndex_ ||
            (expectedFrame == 0 && !sprite_.has_value()))) {
        targetFrameIndex_ = getFrame(window);
    }
    if (!targetFrameIndex_.has_value()) {
        finished_ = true;
    }
    updateSpriteLayout(window);
}

std::optional<int> VideoPlayerImpl::getFrame(sf::RenderWindow& window) {
    if (!decoder_.readFrame()) {
        sprite_.reset();
        return std::nullopt;
    }
    if (!texture_.has_value()) {
        const sf::Vector2u frameSize{
            static_cast<unsigned int>(decoder_.width()),
            static_cast<unsigned int>(decoder_.height())};
        texture_.emplace(frameSize);
        sprite_.emplace(*texture_);
    }
    texture_->update(decoder_.rgbaFrame().data());
    return decoder_.frameIndex() - 1;
}

void VideoPlayerImpl::updateSpriteLayout(const sf::RenderWindow& window) {
    if (!sprite_.has_value() || !texture_.has_value()) {
        return;
    }
    const sf::Vector2u frameSize = texture_->getSize();
    const sf::View view = window.getView();
    const sf::Vector2f viewSize = view.getSize();
    const float scale = std::min(viewSize.x / static_cast<float>(frameSize.x),
                                 viewSize.y / static_cast<float>(frameSize.y));
    sprite_->setScale({scale, scale});
    sprite_->setOrigin({static_cast<float>(frameSize.x) / 2.0f,
                        static_cast<float>(frameSize.y) / 2.0f});
    sprite_->setPosition(view.getCenter());
}

void runVideoPlayback(const std::string& path, bool mute, bool skipable) {
    VideoPlayerImpl(path, mute, skipable).play();
}
#else
void runVideoPlayback(const std::string&, bool, bool) {
    std::cerr << "Video playback is disabled for this project. Enable FFmpeg "
                 "when creating the project.\n";
}
#endif

}  // namespace ludork::video
