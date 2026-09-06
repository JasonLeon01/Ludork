#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <SFML/Audio/Sound.hpp>

class SoundFilter;

class LUDORK_ENGINE_API ActorAudioService {
public:
    virtual ~ActorAudioService();
    virtual std::shared_ptr<sf::Sound> playSoundEffect(
        const std::string& filename, const SoundFilter& filter) = 0;
    virtual void setSoundFilter(const std::shared_ptr<sf::Sound>& sound,
                                const SoundFilter& filter) = 0;
};

LUDORK_ENGINE_API void setActorAudioService(ActorAudioService* service);
