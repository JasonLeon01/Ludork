#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API CurveInfinityMode {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string CONSTANT;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string LINEAR;
};
