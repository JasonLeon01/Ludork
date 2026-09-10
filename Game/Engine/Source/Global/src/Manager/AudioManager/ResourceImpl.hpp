#pragma once

#include <memory>
#include <string>

namespace sf {
class SoundBuffer;
}

namespace ludork::global::audio_manager_impl {

struct AudioImpl;

void retainBuffer(AudioImpl& impl, const std::string& filePath,
                  const std::shared_ptr<sf::SoundBuffer>& buffer);
std::shared_ptr<sf::SoundBuffer> releaseBuffer(AudioImpl& impl,
                                               const std::string& filePath);

}  // namespace ludork::global::audio_manager_impl
