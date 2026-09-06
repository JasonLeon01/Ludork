#include "AudioImpl.hpp"

namespace ludork::global::audio_manager_impl {

AudioImpl& audioImpl() {
    static AudioImpl impl;
    return impl;
}

}  // namespace ludork::global::audio_manager_impl
