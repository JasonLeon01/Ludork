#include <Manager/ManagedSound.hpp>
#include "ManagedAudioSource/AudioLifecycle.hpp"
#include "ManagedAudioSource/EffectImpl.hpp"
#include "ManagedAudioSource/AudioCallbackImpl.hpp"
#include <Manager/AudioEffectControl.hpp>

#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>

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
    managed_audio_source_impl::RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectImpl_->cancel();
        sf::Sound::stop();
        effectImpl_->clear();
        sf::Sound::setEffectProcessor({});
        effectImpl_->waitForCallbacks();
        reclaimed = effectImpl_->takeAllRetired();
    }
}

void ManagedSound::play() {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectImpl_->preparePlaying();
    sf::Sound::play();
    effectImpl_->markPlaying();
}

void ManagedSound::pause() {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    sf::Sound::pause();
}

void ManagedSound::stop() {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    managed_audio_source_impl::RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectImpl_->cancel();
        sf::Sound::stop();
        effectImpl_->clear();
        effectImpl_->waitForCallbacks();
        reclaimed = effectImpl_->takeAllRetired();
    }
}

void ManagedSound::setEffectProcessor(EffectProcessor effectProcessor) {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    managed_audio_source_impl::RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectImpl_->replace(std::move(effectProcessor));
        effectImpl_->waitForCallbacks();
        reclaimed = effectImpl_->takeAllRetired();
    }
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
    managed_audio_source_impl::RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectImpl_->abort();
        if (!managed_audio_source_impl::isCallbackThread()) {
            effectImpl_->waitForCallbacks();
            reclaimed = effectImpl_->takeAllRetired();
        }
    }
}

void ManagedSound::notifyNaturalInputEnded() noexcept {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    if (sf::Sound::getStatus() == sf::SoundSource::Status::Stopped &&
        !effectImpl_->wasExplicitlyStopped()) {
        effectImpl_->notifyNaturalInputEnded();
    }
}

bool ManagedSound::isNaturalInputDrained() const noexcept {
    return effectImpl_->isNaturalInputDrained();
}

bool ManagedSound::wasExplicitlyStopped() const noexcept {
    return effectImpl_->wasExplicitlyStopped();
}

}  // namespace ludork::global::audio
