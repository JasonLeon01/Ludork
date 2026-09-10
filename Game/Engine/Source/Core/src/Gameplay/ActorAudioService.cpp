#include <Gameplay/ActorAudioService.hpp>

#include "Actor/AudioService.hpp"

ActorAudioService::~ActorAudioService() = default;

void setActorAudioService(ActorAudioService* value) {
    actorAudioService() = value;
}
