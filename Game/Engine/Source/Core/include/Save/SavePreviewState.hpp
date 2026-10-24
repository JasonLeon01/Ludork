#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API SavePreviewState {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string IDLE;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string LOADING;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string EMPTY;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string READY;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string FAILED;
};
