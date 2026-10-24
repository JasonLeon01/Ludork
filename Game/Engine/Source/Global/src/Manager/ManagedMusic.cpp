#include <Manager/ManagedMusic.hpp>
#include "ManagedAudioSource/AudioLifecycle.hpp"
#include "ManagedAudioSource/EffectImpl.hpp"
#include <Manager/AudioEffectControl.hpp>

#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>

#include <mutex>
#include <utility>

namespace ludork::global::audio {

ManagedAssetStreamOwner::ManagedAssetStreamOwner() = default;
ManagedAssetStreamOwner::~ManagedAssetStreamOwner() = default;

ManagedMusic::ManagedMusic()
    : effectImpl_(
          std::make_unique<managed_audio_source_impl::EffectImpl>(*this)) {
    sf::Music::setEffectProcessor(effectImpl_->makeTrampoline());
}

ManagedMusic::~ManagedMusic() {
    managed_audio_source_impl::stopManagedAudio<sf::Music>(
        *this, *effectImpl_, mutationMutex_, true);
}

void ManagedMusic::play() {
    managed_audio_source_impl::playManagedAudio<sf::Music>(*this, *effectImpl_,
                                                           mutationMutex_);
}

void ManagedMusic::pause() {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    sf::Music::pause();
}

void ManagedMusic::stop() {
    managed_audio_source_impl::requireManagedAudioLifecycleCaller();
    managed_audio_source_impl::stopManagedAudio<sf::Music>(
        *this, *effectImpl_, mutationMutex_, false);
}

void ManagedMusic::setEffectProcessor(EffectProcessor effectProcessor) {
    managed_audio_source_impl::replaceManagedAudioEffect(
        *effectImpl_, mutationMutex_, std::move(effectProcessor));
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
    effectImpl_->begin(control == nullptr ? nullptr : control->stateToken());
}

void ManagedMusic::finishEffectAttachment() {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectImpl_->finish();
}

void ManagedMusic::abortEffectAttachment() {
    managed_audio_source_impl::abortManagedAudioEffectAttachment(
        *effectImpl_, mutationMutex_);
}

void ManagedMusic::notifyNaturalInputEnded() noexcept {
    managed_audio_source_impl::notifyManagedAudioNaturalInputEnded<sf::Music>(
        *this, *effectImpl_, mutationMutex_);
}

bool ManagedMusic::isNaturalInputDrained() const noexcept {
    return effectImpl_->isNaturalInputDrained();
}

bool ManagedMusic::wasExplicitlyStopped() const noexcept {
    return effectImpl_->wasExplicitlyStopped();
}

}  // namespace ludork::global::audio
