#include <Manager/ActorAudioBridge.hpp>

#include <Filters/SoundFilter.hpp>
#include <Gameplay/Actor.hpp>
#include <Manager/AudioManager.hpp>

namespace {

class GlobalActorAudioService final : public ActorAudioService {
public:
    std::shared_ptr<sf::Sound> playSoundEffect(
        const std::string& filename, const SoundFilter& filter) override {
        return AudioManager::playSound(filename, &filter);
    }

    void setSoundFilter(const std::shared_ptr<sf::Sound>& sound,
                        const SoundFilter& filter) override {
        AudioManager::setSoundFilter(sound, filter);
    }
};

}  // namespace

void initializeActorAudioBridge() {
    static GlobalActorAudioService service;
    setActorAudioService(&service);
}

void shutdownActorAudioBridge() noexcept {
    setActorAudioService(nullptr);
}
