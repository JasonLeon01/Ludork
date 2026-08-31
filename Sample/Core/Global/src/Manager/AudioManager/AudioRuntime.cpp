#include "AudioRuntime.hpp"

namespace ludork::global::audio_manager_impl {

AudioRuntime& audioRuntime() {
    static AudioRuntime runtime;
    return runtime;
}

}  // namespace ludork::global::audio_manager_impl
