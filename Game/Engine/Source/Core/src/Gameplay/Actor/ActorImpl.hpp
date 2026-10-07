#pragma once

#include "AudioImpl.hpp"
#include "MovementImpl.hpp"
#include "SpatialImpl.hpp"
#include "VisualImpl.hpp"

namespace ludork::engine::actor_impl {

struct ActorImpl {
    std::vector<std::shared_ptr<AsyncOperation>> asyncOperations;
    MovementImpl movement;
    AudioImpl audio;
    mutable SpatialImpl spatial;
    mutable VisualImpl visual;
};

}  // namespace ludork::engine::actor_impl
