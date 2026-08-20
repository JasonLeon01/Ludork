#include <Gameplay/Actor.hpp>

#include <SFML/Audio/Listener.hpp>

#include <algorithm>
#include <cmath>

namespace {
ActorAudioService* actorAudioService = nullptr;
}  // namespace

ActorAudioService::~ActorAudioService() = default;

void setActorAudioService(ActorAudioService* service) {
    actorAudioService = service;
}

std::shared_ptr<AutoSoundParams> Actor::getAutoSoundParams() const {
    return autoSoundParams_;
}

void Actor::setAutoSoundParams(const AutoSoundParams& params) {
    autoSoundParams_ = std::make_shared<AutoSoundParams>(params);
}

void Actor::normaliseAutoSoundParams() {
    autoSoundParams_ =
        autoSoundParams_ ? std::make_shared<AutoSoundParams>(*autoSoundParams_)
                         : std::make_shared<AutoSoundParams>();
}

float Actor::autoSoundListenerDistance() const {
    const sf::Vector3f listenerPosition = sf::Listener::getPosition();
    const sf::Vector2f actorPosition = getPosition();
    const float x = actorPosition.x - listenerPosition.x;
    const float y = actorPosition.y - listenerPosition.y;
    return std::sqrt(x * x + y * y);
}

void Actor::updateAutoSound(float deltaTime) {
    normaliseAutoSoundParams();
    if (autoSound.empty()) {
        stopAutoSound();
        autoSoundCooldown_ = 0.0f;
        return;
    }
    const float stopDistance = autoSoundParams_->maxDistance;
    if (stopDistance > 0.0f) {
        const float distance = autoSoundListenerDistance();
        const float startDistance = stopDistance * 0.85f;
        if (distance > stopDistance) {
            stopAutoSound();
            autoSoundCooldown_ = 0.0f;
            return;
        }
        if (!autoSoundObject_ && distance > startDistance) {
            return;
        }
    }
    if (autoSoundObject_) {
        if (autoSoundObject_->getStatus() == sf::SoundSource::Status::Stopped) {
            autoSoundObject_.reset();
            autoSoundCooldown_ = std::max(0.0f, autoSoundInterval);
        } else {
            applyAutoSoundParams();
            return;
        }
    }
    if (autoSoundCooldown_ > 0.0f) {
        autoSoundCooldown_ = std::max(0.0f, autoSoundCooldown_ - deltaTime);
        return;
    }
    playAutoSound();
}

void Actor::playAutoSound() {
    if (actorAudioService == nullptr) {
        return;
    }
    autoSoundObject_ =
        actorAudioService->playSoundEffect(autoSound, buildAutoSoundFilter());
    if (!autoSoundObject_) {
        return;
    }
    const sf::Vector2f position = getPosition();
    autoSoundLastPosition_ = sf::Vector3f(position.x, position.y, 0.0f);
}

void Actor::stopAutoSound() {
    if (!autoSoundObject_) {
        return;
    }
    if (autoSoundObject_->getStatus() != sf::SoundSource::Status::Stopped) {
        autoSoundObject_->stop();
    }
    autoSoundObject_.reset();
    autoSoundLastPosition_.reset();
}

void Actor::applyAutoSoundParams() {
    if (!autoSoundObject_ || actorAudioService == nullptr) {
        return;
    }
    const sf::Vector2f position = getPosition();
    const sf::Vector3f newPosition(position.x, position.y, 0.0f);
    if (autoSoundLastPosition_.has_value() &&
        *autoSoundLastPosition_ == newPosition) {
        return;
    }
    autoSoundLastPosition_ = newPosition;
    actorAudioService->setSoundFilter(autoSoundObject_, buildAutoSoundFilter());
}

SoundFilter Actor::buildAutoSoundFilter() const {
    SoundFilter filter;
    const sf::Vector2f position = getPosition();
    filter.volume = autoSoundParams_->volume;
    filter.spatial = true;
    filter.position = sf::Vector3f(position.x, position.y, 0.0f);
    filter.relativeToListener = false;
    filter.minDistance = autoSoundParams_->minDistance;
    filter.attenuation = autoSoundParams_->attenuation;
    if (autoSoundParams_->loop) {
        filter.loop = true;
    }
    if (autoSoundParams_->maxDistance > 0.0f) {
        filter.maxDistance = autoSoundParams_->maxDistance;
    }
    return filter;
}
