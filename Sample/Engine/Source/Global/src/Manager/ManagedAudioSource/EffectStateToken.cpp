#include "EffectStateToken.hpp"

namespace ludork::global::managed_audio_source_impl {

bool EffectStateToken::isCancelled() const noexcept {
    return state_.load(std::memory_order_acquire) == EffectState::Cancelled;
}

void EffectStateToken::beginTail() noexcept {
    EffectState expected = EffectState::Drained;
    state_.compare_exchange_strong(expected, EffectState::TailPending,
                                   std::memory_order_acq_rel,
                                   std::memory_order_acquire);
}

void EffectStateToken::finishTail() noexcept {
    EffectState expected = EffectState::TailPending;
    state_.compare_exchange_strong(expected, EffectState::Drained,
                                   std::memory_order_acq_rel,
                                   std::memory_order_acquire);
}

void EffectStateToken::cancel() noexcept {
    state_.store(EffectState::Cancelled, std::memory_order_release);
}

bool EffectStateToken::isDrained() const noexcept {
    return state_.load(std::memory_order_acquire) != EffectState::TailPending;
}

}  // namespace ludork::global::managed_audio_source_impl
