#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS(name = "Clock")
class ModifiedClock : public sf::Clock {
public:
    BIND_INIT()
    ModifiedClock() = default;

    BIND_METHOD()
    float v_reset();

    BIND_METHOD()
    float v_restart();
};
