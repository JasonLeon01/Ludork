#pragma once

#include <memory>
#include <string>

namespace sf {
class SoundBuffer;
}

namespace ludork::global::audio_manager_impl {

struct AudioRuntime;

void retainBuffer(AudioRuntime& runtime, const std::string& filePath,
                  const std::shared_ptr<sf::SoundBuffer>& buffer);
std::shared_ptr<sf::SoundBuffer> releaseBuffer(AudioRuntime& runtime,
                                               const std::string& filePath);

}  // namespace ludork::global::audio_manager_impl
