#pragma once

#include "AudioImpl.hpp"
#include "MovementImpl.hpp"
#include "SpatialImpl.hpp"
#include "VisualImpl.hpp"

namespace ludork::engine::actor_impl {

struct ActorImpl {
    MovementImpl movement;
    AudioImpl audio;
    mutable SpatialImpl spatial;
    mutable VisualImpl visual;
};

}  // namespace ludork::engine::actor_impl
