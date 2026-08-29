#include "AudioService.hpp"

#include <Gameplay/Actor/ActorApiTypes.hpp>

namespace {
ActorAudioService* service = nullptr;
}

ActorAudioService*& actorAudioService() {
    return service;
}

ActorAudioService::~ActorAudioService() = default;

void setActorAudioService(ActorAudioService* value) {
    service = value;
}
