#include "ShutdownBarrier.hpp"

#include "AudioRuntime.hpp"

#include <mutex>

namespace ludork::global::audio_manager_impl {

CreationScope::CreationScope(AudioRuntime& runtime) noexcept
    : runtime_(&runtime) {}

CreationScope::~CreationScope() {
    if (!active_) {
        return;
    }
    {
        const std::lock_guard<std::recursive_mutex> lock(runtime_->mutex);
        --runtime_->creationsInFlight;
    }
    runtime_->creationCondition.notify_all();
}

void CreationScope::activate() noexcept {
    ++runtime_->creationsInFlight;
    active_ = true;
}

}  // namespace ludork::global::audio_manager_impl
