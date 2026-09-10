#include "ShutdownBarrier.hpp"

#include "AudioImpl.hpp"

#include <mutex>

namespace ludork::global::audio_manager_impl {

CreationScope::CreationScope(AudioImpl& impl) noexcept : impl_(&impl) {}

CreationScope::~CreationScope() {
    if (!active_) {
        return;
    }
    {
        const std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
        --impl_->creationsInFlight;
    }
    impl_->creationCondition.notify_all();
}

void CreationScope::activate() noexcept {
    ++impl_->creationsInFlight;
    active_ = true;
}

}  // namespace ludork::global::audio_manager_impl
