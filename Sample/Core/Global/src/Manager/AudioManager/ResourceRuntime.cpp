#include "ResourceRuntime.hpp"

#include "AudioRuntime.hpp"

#include <utility>

namespace ludork::global::audio_manager_impl {

void retainBuffer(AudioRuntime& runtime, const std::string& filePath,
                  const std::shared_ptr<sf::SoundBuffer>& buffer) {
    runtime.soundBuffers[filePath] = buffer;
    ++runtime.soundBufferCounts[filePath];
}

std::shared_ptr<sf::SoundBuffer> releaseBuffer(AudioRuntime& runtime,
                                               const std::string& filePath) {
    const auto iterator = runtime.soundBufferCounts.find(filePath);
    if (iterator == runtime.soundBufferCounts.end()) {
        return nullptr;
    }
    if (iterator->second > 1) {
        --iterator->second;
        return nullptr;
    }
    runtime.soundBufferCounts.erase(iterator);
    const auto bufferIterator = runtime.soundBuffers.find(filePath);
    if (bufferIterator == runtime.soundBuffers.end()) {
        return nullptr;
    }
    std::shared_ptr<sf::SoundBuffer> buffer = std::move(bufferIterator->second);
    runtime.soundBuffers.erase(bufferIterator);
    return buffer;
}

}  // namespace ludork::global::audio_manager_impl
