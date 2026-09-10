#include "AudioImpl.hpp"
#include <Filters/SoundFilter.hpp>
#include <Gameplay/AutoSoundParams.hpp>
#include <Gameplay/ActorAudioService.hpp>

#include "AudioService.hpp"

#include <SFML/Audio/Listener.hpp>

#include <algorithm>
#include <cmath>

namespace ludork::engine::actor_impl {

std::shared_ptr<AutoSoundParams> AudioImpl::getParams() const {
    return params_;
}

void AudioImpl::setParams(const AutoSoundParams& params) {
    params_ = std::make_shared<AutoSoundParams>(params);
}

void AudioImpl::normaliseParams() {
    params_ = params_ ? std::make_shared<AutoSoundParams>(*params_)
                      : std::make_shared<AutoSoundParams>();
}

void AudioImpl::update(float deltaTime, const std::string& sound,
                       const float& interval,
                       const PositionReader& readPosition) {
    if (sound.empty()) {
        stop();
        cooldown_ = 0.0f;
        return;
    }
    const float stopDistance = params_->maxDistance;
    if (stopDistance > 0.0f) {
        const float distance = listenerDistance(readPosition());
        const float startDistance = stopDistance * 0.85f;
        if (distance > stopDistance) {
            stop();
            cooldown_ = 0.0f;
            return;
        }
        if (!sound_ && distance > startDistance) {
            return;
        }
    }
    if (sound_) {
        if (sound_->getStatus() == sf::SoundSource::Status::Stopped) {
            sound_.reset();
            cooldown_ = std::max(0.0f, interval);
        } else {
            applyParams(readPosition);
            return;
        }
    }
    if (cooldown_ > 0.0f) {
        cooldown_ = std::max(0.0f, cooldown_ - deltaTime);
        return;
    }
    play(sound, readPosition);
}

void AudioImpl::play(const std::string& sound,
                     const PositionReader& readPosition) {
    if (actorAudioService() == nullptr) {
        return;
    }
    sound_ =
        actorAudioService()->playSoundEffect(sound, buildFilter(readPosition));
    if (!sound_) {
        return;
    }
    const sf::Vector2f position = readPosition();
    lastPosition_ = sf::Vector3f(position.x, position.y, 0.0f);
}

void AudioImpl::stop() {
    if (!sound_) {
        return;
    }
    if (sound_->getStatus() != sf::SoundSource::Status::Stopped) {
        sound_->stop();
    }
    sound_.reset();
    lastPosition_.reset();
}

void AudioImpl::applyParams(const PositionReader& readPosition) {
    if (!sound_ || actorAudioService() == nullptr) {
        return;
    }
    const sf::Vector2f position = readPosition();
    const sf::Vector3f newPosition(position.x, position.y, 0.0f);
    if (lastPosition_.has_value() && *lastPosition_ == newPosition) {
        return;
    }
    lastPosition_ = newPosition;
    actorAudioService()->setSoundFilter(sound_, buildFilter(readPosition));
}

SoundFilter AudioImpl::buildFilter(const PositionReader& readPosition) const {
    return buildSoundFilter(*params_, readPosition());
}

float listenerDistance(const sf::Vector2f& position) {
    const sf::Vector3f listenerPosition = sf::Listener::getPosition();
    const float x = position.x - listenerPosition.x;
    const float y = position.y - listenerPosition.y;
    return std::sqrt(x * x + y * y);
}

SoundFilter buildSoundFilter(const AutoSoundParams& params,
                             const sf::Vector2f& position) {
    SoundFilter filter;
    filter.volume = params.volume;
    filter.spatial = true;
    filter.position = sf::Vector3f(position.x, position.y, 0.0f);
    filter.relativeToListener = false;
    filter.minDistance = params.minDistance;
    filter.attenuation = params.attenuation;
    if (params.loop) {
        filter.loop = true;
    }
    if (params.maxDistance > 0.0f) {
        filter.maxDistance = params.maxDistance;
    }
    return filter;
}

}  // namespace ludork::engine::actor_impl
