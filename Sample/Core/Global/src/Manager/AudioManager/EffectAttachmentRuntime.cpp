#include "EffectAttachmentRuntime.hpp"

#include <SFML/Audio/PlaybackDevice.hpp>

#include <optional>
#include <stdexcept>

namespace ludork::global::audio_manager_impl {

namespace {

template <typename Source>
std::shared_ptr<AudioEffectControl> attach(
    Source& source, const ludork::global::audio::AudioEffectAttacher& effect) {
    if (!effect) {
        return nullptr;
    }
    const std::optional<std::uint32_t> sampleRate =
        sf::PlaybackDevice::getDeviceSampleRate();
    if (!sampleRate.has_value() || *sampleRate == 0) {
        throw std::runtime_error(
            "Audio playback device sample rate is unavailable");
    }
    std::shared_ptr<AudioEffectControl> control =
        std::make_shared<AudioEffectControl>();
    source.beginEffectAttachment(control);
    try {
        effect(source, control, *sampleRate);
        source.finishEffectAttachment();
    } catch (...) {
        source.abortEffectAttachment();
        throw;
    }
    return control;
}

}  // namespace

std::shared_ptr<AudioEffectControl> attachEffect(
    ludork::global::audio::ManagedSound& source,
    const ludork::global::audio::AudioEffectAttacher& effect) {
    return attach(source, effect);
}

std::shared_ptr<AudioEffectControl> attachEffect(
    ludork::global::audio::ManagedMusic& source,
    const ludork::global::audio::AudioEffectAttacher& effect) {
    return attach(source, effect);
}

}  // namespace ludork::global::audio_manager_impl
