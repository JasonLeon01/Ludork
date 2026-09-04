#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>

#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>

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
