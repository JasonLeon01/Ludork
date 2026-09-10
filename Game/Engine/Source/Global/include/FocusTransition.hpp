#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS()
class FocusTransition {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string DIRECTIONAL;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string EXPLICIT;
};
