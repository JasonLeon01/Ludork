#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_CLASS(metadata = false)
class LUDORK_GLOBAL_API GameplayAttributeChangeSource {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string BASE;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string EFFECT;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string CONSTRAINT;
};
