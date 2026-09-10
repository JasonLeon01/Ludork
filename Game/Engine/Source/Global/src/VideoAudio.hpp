#pragma once

#if LUDORK_HAS_FFMPEG
#include <SFML/Audio/SoundChannel.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ludork::video {

struct AudioData {
    std::vector<std::int16_t> samples;
    unsigned int channelCount = 0;
    unsigned int sampleRate = 0;
    std::vector<sf::SoundChannel> channelMap;
};

AudioData extractAudio(const std::string& path);

}  // namespace ludork::video
#endif
