#include "AudioService.hpp"
#include <Gameplay/ActorAudioService.hpp>

namespace {
ActorAudioService* service = nullptr;
}

ActorAudioService*& actorAudioService() {
    return service;
}
