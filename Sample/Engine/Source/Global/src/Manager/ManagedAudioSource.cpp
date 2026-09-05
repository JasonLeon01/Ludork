#include <Manager/ManagedAudioSource.hpp>

#include "AudioEffectLuaRuntime.hpp"
#include "ManagedAudioSource/AudioCallbackRuntime.hpp"
#include "ManagedAudioSource/EffectRuntime.hpp"

#include <Runtime/AssetStore.hpp>

#include <mutex>
#include <stdexcept>
#include <utility>

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

namespace ludork::global::audio {

namespace {

using managed_audio_source_impl::EffectRuntime;
using managed_audio_source_impl::RetiredProcessorGenerations;

void requireManagedAudioLifecycleCaller() {
    if (managed_audio_source_impl::isCallbackThread()) {
        throw std::logic_error(
            "Managed audio source lifecycle cannot change from an effect "
            "processor");
    }
}

const sf::SoundBuffer& requireSoundBuffer(
    const std::shared_ptr<const sf::SoundBuffer>& buffer) {
    if (buffer == nullptr) {
        throw std::invalid_argument("Managed sound buffer is required");
    }
    return *buffer;
}

}  // namespace

bool isManagedAudioCallbackThread() noexcept {
    return managed_audio_source_impl::isCallbackThread();
}

ManagedSoundBufferOwner::ManagedSoundBufferOwner(
    const std::shared_ptr<const sf::SoundBuffer>& buffer)
    : buffer_(buffer) {
    static_cast<void>(requireSoundBuffer(buffer));
}

ManagedAssetStreamOwner::ManagedAssetStreamOwner() = default;
ManagedAssetStreamOwner::~ManagedAssetStreamOwner() = default;

ManagedSound::ManagedSound(const std::shared_ptr<const sf::SoundBuffer>& buffer)
    : ManagedSoundBufferOwner(buffer),
      sf::Sound(requireSoundBuffer(buffer)),
      effectState_(std::make_unique<EffectRuntime>(*this)) {
    sf::Sound::setEffectProcessor(effectState_->makeTrampoline());
}

ManagedSound::~ManagedSound() {
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Sound::stop();
        effectState_->clear();
        sf::Sound::setEffectProcessor({});
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
}

void ManagedSound::play() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->preparePlaying();
    sf::Sound::play();
    effectState_->markPlaying();
}

void ManagedSound::pause() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    sf::Sound::pause();
}

void ManagedSound::stop() {
    requireManagedAudioLifecycleCaller();
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Sound::stop();
        effectState_->clear();
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
}

void ManagedSound::setEffectProcessor(EffectProcessor effectProcessor) {
    requireManagedAudioLifecycleCaller();
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->replace(std::move(effectProcessor));
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
}

void ManagedSound::beginEffectAttachment(
    const std::shared_ptr<AudioEffectControl>& control) {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->begin(control == nullptr ? nullptr : control->stateToken());
}

void ManagedSound::finishEffectAttachment() {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->finish();
}

void ManagedSound::abortEffectAttachment() {
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->abort();
        if (!managed_audio_source_impl::isCallbackThread()) {
            effectState_->waitForCallbacks();
            reclaimed = effectState_->takeAllRetired();
        }
    }
}

void ManagedSound::notifyNaturalInputEnded() noexcept {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    if (sf::Sound::getStatus() == sf::SoundSource::Status::Stopped &&
        !effectState_->wasExplicitlyStopped()) {
        effectState_->notifyNaturalInputEnded();
    }
}

bool ManagedSound::isNaturalInputDrained() const noexcept {
    return effectState_->isNaturalInputDrained();
}

bool ManagedSound::wasExplicitlyStopped() const noexcept {
    return effectState_->wasExplicitlyStopped();
}

ManagedMusic::ManagedMusic()
    : effectState_(std::make_unique<EffectRuntime>(*this)) {
    sf::Music::setEffectProcessor(effectState_->makeTrampoline());
}

ManagedMusic::~ManagedMusic() {
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Music::stop();
        effectState_->clear();
        sf::Music::setEffectProcessor({});
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
}

void ManagedMusic::play() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->preparePlaying();
    sf::Music::play();
    effectState_->markPlaying();
}

void ManagedMusic::pause() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    sf::Music::pause();
}

void ManagedMusic::stop() {
    requireManagedAudioLifecycleCaller();
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Music::stop();
        effectState_->clear();
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
}

void ManagedMusic::setEffectProcessor(EffectProcessor effectProcessor) {
    requireManagedAudioLifecycleCaller();
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->replace(std::move(effectProcessor));
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
}

bool ManagedMusic::openFromAsset(const std::string& assetPath) {
    std::unique_ptr<ludork::runtime::AssetInputStream> stream =
        ludork::runtime::assetStore().open(assetPath);
    if (!sf::Music::openFromStream(*stream)) {
        return false;
    }
    assetStream_ = std::move(stream);
    return true;
}

void ManagedMusic::beginEffectAttachment(
    const std::shared_ptr<AudioEffectControl>& control) {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->begin(control == nullptr ? nullptr : control->stateToken());
}

void ManagedMusic::finishEffectAttachment() {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->finish();
}

void ManagedMusic::abortEffectAttachment() {
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->abort();
        if (!managed_audio_source_impl::isCallbackThread()) {
            effectState_->waitForCallbacks();
            reclaimed = effectState_->takeAllRetired();
        }
    }
}

void ManagedMusic::notifyNaturalInputEnded() noexcept {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    if (sf::Music::getStatus() == sf::SoundSource::Status::Stopped &&
        !effectState_->wasExplicitlyStopped()) {
        effectState_->notifyNaturalInputEnded();
    }
}

bool ManagedMusic::isNaturalInputDrained() const noexcept {
    return effectState_->isNaturalInputDrained();
}

bool ManagedMusic::wasExplicitlyStopped() const noexcept {
    return effectState_->wasExplicitlyStopped();
}

}  // namespace ludork::global::audio
