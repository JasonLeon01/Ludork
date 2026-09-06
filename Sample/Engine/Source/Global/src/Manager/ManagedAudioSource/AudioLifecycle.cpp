#include "AudioLifecycle.hpp"
#include "AudioCallbackImpl.hpp"

#include <stdexcept>

namespace ludork::global::managed_audio_source_impl {

void requireManagedAudioLifecycleCaller() {
    if (isCallbackThread()) {
        throw std::logic_error(
            "Managed audio source lifecycle cannot change from an effect "
            "processor");
    }
}

}  // namespace ludork::global::managed_audio_source_impl
