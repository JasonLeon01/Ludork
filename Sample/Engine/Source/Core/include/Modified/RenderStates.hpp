#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS(name = "RenderStates")
class ModifiedRenderStates : public sf::RenderStates {
public:
    BIND_INIT()
    ModifiedRenderStates() = default;

    explicit ModifiedRenderStates(const sf::RenderStates& states);

    BIND_METHOD(Pure = true, name = "Default")
    static ModifiedRenderStates Default();
};
