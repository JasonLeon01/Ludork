#pragma once

#include "EffectImpl.hpp"

namespace ludork::global::managed_audio_source_impl {

class ProcessorGeneration {
public:
    static constexpr std::uint32_t CancelledBit = 1U << 31U;
    static constexpr std::uint32_t ActivityMask = CancelledBit - 1U;

    std::shared_ptr<EffectStateToken> control;
    sf::SoundSource::EffectProcessor processor;
    std::shared_ptr<ProcessorGeneration> retiredNext;
    std::atomic_uint32_t gate{0};
    std::atomic_bool endOfStreamComplete{false};
    bool requiresEndOfStreamAcknowledgement = false;

    [[nodiscard]] bool tryAcquireActivity() noexcept;

    void releaseActivity() noexcept;

    void cancel() noexcept;

    void waitUntilInactive() const noexcept;
};

}  // namespace ludork::global::managed_audio_source_impl
