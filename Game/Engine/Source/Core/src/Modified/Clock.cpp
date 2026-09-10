#include <Modified/Clock.hpp>

float ModifiedClock::v_reset() {
    return reset().asSeconds();
}

float ModifiedClock::v_restart() {
    return restart().asSeconds();
}
