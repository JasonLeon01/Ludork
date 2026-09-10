#include <Manager/ManagedAudioSource.hpp>
#include "ManagedAudioSource/AudioCallbackImpl.hpp"

namespace ludork::global::audio {

bool isManagedAudioCallbackThread() noexcept {
    return managed_audio_source_impl::isCallbackThread();
}

}  // namespace ludork::global::audio
