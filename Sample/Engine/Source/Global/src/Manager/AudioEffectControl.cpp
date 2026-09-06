#include <Manager/AudioEffectControl.hpp>
#include "AudioEffectLuaRuntime.hpp"
#include "ManagedAudioSource/EffectImpl.hpp"

AudioEffectControl::AudioEffectControl()
    : state_(std::make_shared<
             ludork::global::managed_audio_source_impl::EffectStateToken>()) {}

AudioEffectControl::~AudioEffectControl() = default;

bool AudioEffectControl::isCancelled() const noexcept {
    return state_->isCancelled();
}

void AudioEffectControl::beginTail() noexcept {
    state_->beginTail();
}

void AudioEffectControl::finishTail() noexcept {
    state_->finishTail();
}

void AudioEffectControl::attachLuaProcessor(sf::SoundSource& source,
                                            const std::string& name,
                                            std::uint32_t sampleRate) {
    source.setEffectProcessor(
        ludork::global::audio::createLuaAudioEffectProcessor(
            name, shared_from_this(), sampleRate));
}

void AudioEffectControl::cancel() noexcept {
    state_->cancel();
}

bool AudioEffectControl::isDrained() const noexcept {
    return state_->isDrained();
}

const std::shared_ptr<
    ludork::global::managed_audio_source_impl::EffectStateToken>&
AudioEffectControl::stateToken() const noexcept {
    return state_;
}
