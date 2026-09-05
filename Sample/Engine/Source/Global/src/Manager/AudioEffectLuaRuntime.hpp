#pragma once

#include <SFML/Audio/SoundSource.hpp>

#include <cstdint>
#include <memory>
#include <string>

struct lua_State;
class AudioEffectControl;

namespace ludork::global::audio {

void initializeAudioEffectLuaRuntime(lua_State* mainState);
void shutdownAudioEffectLuaRuntime() noexcept;

[[nodiscard]] sf::SoundSource::EffectProcessor createLuaAudioEffectProcessor(
    const std::string& name, const std::shared_ptr<AudioEffectControl>& control,
    std::uint32_t sampleRate);

void throwDeferredAudioEffectError();

}  // namespace ludork::global::audio
