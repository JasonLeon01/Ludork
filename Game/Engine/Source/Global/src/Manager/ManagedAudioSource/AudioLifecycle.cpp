#include "AudioLifecycle.hpp"
#include "AudioCallbackImpl.hpp"

#include <stdexcept>
#include <utility>

namespace ludork::global::managed_audio_source_impl {

void requireManagedAudioLifecycleCaller() {
    if (isCallbackThread()) {
        throw std::logic_error(
            "Managed audio source lifecycle cannot change from an effect "
            "processor");
    }
}

void replaceManagedAudioEffect(
    EffectImpl& effect, std::mutex& mutationMutex,
    sf::SoundSource::EffectProcessor effectProcessor) {
    requireManagedAudioLifecycleCaller();
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex);
        effect.replace(std::move(effectProcessor));
        effect.waitForCallbacks();
        reclaimed = effect.takeAllRetired();
    }
}

void abortManagedAudioEffectAttachment(EffectImpl& effect,
                                       std::mutex& mutationMutex) {
    RetiredProcessorGenerations reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex);
        effect.abort();
        if (!isCallbackThread()) {
            effect.waitForCallbacks();
            reclaimed = effect.takeAllRetired();
        }
    }
}

}  // namespace ludork::global::managed_audio_source_impl
