#include "ResourceImpl.hpp"

#include "AudioImpl.hpp"

#include <utility>

namespace ludork::global::audio_manager_impl {

void retainBuffer(AudioImpl& impl, const std::string& filePath,
                  const std::shared_ptr<sf::SoundBuffer>& buffer) {
    impl.soundBuffers[filePath] = buffer;
    ++impl.soundBufferCounts[filePath];
}

std::shared_ptr<sf::SoundBuffer> releaseBuffer(AudioImpl& impl,
                                               const std::string& filePath) {
    const auto iterator = impl.soundBufferCounts.find(filePath);
    if (iterator == impl.soundBufferCounts.end()) {
        return nullptr;
    }
    if (iterator->second > 1) {
        --iterator->second;
        return nullptr;
    }
    impl.soundBufferCounts.erase(iterator);
    const auto bufferIterator = impl.soundBuffers.find(filePath);
    if (bufferIterator == impl.soundBuffers.end()) {
        return nullptr;
    }
    std::shared_ptr<sf::SoundBuffer> buffer = std::move(bufferIterator->second);
    impl.soundBuffers.erase(bufferIterator);
    return buffer;
}

}  // namespace ludork::global::audio_manager_impl
