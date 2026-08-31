#pragma once

#include <Manager/ManagedAudioSource.hpp>

#include <memory>

namespace ludork::global::audio_manager_impl {

std::shared_ptr<AudioEffectControl> attachEffect(
    ludork::global::audio::ManagedSound& source,
    const ludork::global::audio::AudioEffectAttacher& effect);
std::shared_ptr<AudioEffectControl> attachEffect(
    ludork::global::audio::ManagedMusic& source,
    const ludork::global::audio::AudioEffectAttacher& effect);

}  // namespace ludork::global::audio_manager_impl
