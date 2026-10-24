#include <Manager/ManagedSound.hpp>
#include "ManagedAudioSource/AudioLifecycle.hpp"
#include "ManagedAudioSource/EffectImpl.hpp"
#include <Manager/AudioEffectControl.hpp>

#include <mutex>
#include <stdexcept>
#include <utility>

namespace ludork::global::audio {

namespace {

const sf::SoundBuffer& requireSoundBuffer(
    const std::shared_ptr<const sf::SoundBuffer>& buffer) {
    if (buffer == nullptr) {
        throw std::invalid_argument("Managed sound buffer is required");
    }
    return *buffer;
}

}  // namespace

ManagedSoundBufferOwner::ManagedSoundBufferOwner(
    const std::shared_ptr<const sf::SoundBuffer>& buffer)
    : buffer_(buffer) {
    static_cast<void>(requireSoundBuffer(buffer));
}

ManagedSound::ManagedSound(const std::shared_ptr<const sf::SoundBuffer>& buffer)
    : ManagedSoundBufferOwner(buffer),
      sf::Sound(requireSoundBuffer(buffer)),
      effectImpl_(
          std::make_unique<managed_audio_source_impl::EffectImpl>(*this)) {
    sf::Sound::setEffectProcessor(effectImpl_->makeTrampoline());
}

ManagedSound::~ManagedSound() {
    managed_audio_source_impl::stopManagedAudio<sf::Sound>(
        *this, *effectImpl_, mutationMutex_, true);
}

void ManagedSound::play() {
    managed_audio_source_impl::playManagedAudio<sf::Sound>(*this, *effectImpl_,
                                                           mutationMutex_);
}

void ManagedSound::pause() {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    sf::Sound::pause();
}

void ManagedSound::stop() {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    managed_audio_source_impl::stopManagedAudio<sf::Sound>(
        *this, *effectImpl_, mutationMutex_, false);
}

void ManagedSound::setEffectProcessor(EffectProcessor effectProcessor) {
    managed_audio_source_impl::replaceManagedAudioEffect(
        *effectImpl_, mutationMutex_, std::move(effectProcessor));
}

void ManagedSound::beginEffectAttachment(
    const std::shared_ptr<AudioEffectControl>& control) {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectImpl_->begin(control == nullptr ? nullptr : control->stateToken());
}

void ManagedSound::finishEffectAttachment() {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectImpl_->finish();
}

void ManagedSound::abortEffectAttachment() {
    managed_audio_source_impl::abortManagedAudioEffectAttachment(
        *effectImpl_, mutationMutex_);
}

void ManagedSound::notifyNaturalInputEnded() noexcept {
    managed_audio_source_impl::notifyManagedAudioNaturalInputEnded<sf::Sound>(
        *this, *effectImpl_, mutationMutex_);
}

bool ManagedSound::isNaturalInputDrained() const noexcept {
    return effectImpl_->isNaturalInputDrained();
}

bool ManagedSound::wasExplicitlyStopped() const noexcept {
    return effectImpl_->wasExplicitlyStopped();
}

}  // namespace ludork::global::audio
