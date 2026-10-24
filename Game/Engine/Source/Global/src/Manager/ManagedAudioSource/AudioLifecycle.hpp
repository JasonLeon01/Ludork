#pragma once

#include "EffectImpl.hpp"

#include <SFML/Audio/SoundSource.hpp>

#include <mutex>

namespace ludork::global::managed_audio_source_impl {

void requireManagedAudioLifecycleCaller();

template <typename Source>
void playManagedAudio(Source& source, EffectImpl& effect,
                      std::mutex& mutationMutex) {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex);
    effect.preparePlaying();
    source.Source::play();
    effect.markPlaying();
}

template <typename Source>
void stopManagedAudio(Source& source, EffectImpl& effect,
                      std::mutex& mutationMutex, bool detachProcessor) {
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex);
        effect.cancel();
        source.Source::stop();
        effect.clear();
        if (detachProcessor) {
            source.Source::setEffectProcessor({});
        }
        effect.waitForCallbacks();
        reclaimed = effect.takeAllRetired();
    }
}

void replaceManagedAudioEffect(
    EffectImpl& effect, std::mutex& mutationMutex,
    sf::SoundSource::EffectProcessor effectProcessor);
void abortManagedAudioEffectAttachment(EffectImpl& effect,
                                       std::mutex& mutationMutex);

template <typename Source>
void notifyManagedAudioNaturalInputEnded(Source& source, EffectImpl& effect,
                                         std::mutex& mutationMutex) noexcept {
    const std::lock_guard<std::mutex> lock(mutationMutex);
    if (source.Source::getStatus() == sf::SoundSource::Status::Stopped &&
        !effect.wasExplicitlyStopped()) {
        effect.notifyNaturalInputEnded();
    }
}

}  // namespace ludork::global::managed_audio_source_impl
