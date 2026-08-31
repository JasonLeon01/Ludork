#pragma once

#include <atomic>
#include <cstdint>

namespace ludork::global::managed_audio_source_impl {

enum class EffectState : std::uint8_t {
    Drained,
    TailPending,
    Cancelled
};

class EffectStateToken {
public:
    bool isCancelled() const noexcept;
    void beginTail() noexcept;
    void finishTail() noexcept;
    void cancel() noexcept;
    bool isDrained() const noexcept;

private:
    std::atomic<EffectState> state_{EffectState::Drained};
};

}  // namespace ludork::global::managed_audio_source_impl
