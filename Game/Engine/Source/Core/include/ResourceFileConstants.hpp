#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API ResourceFileConstants {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string DATA_EXTENSION;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string ENCRYPTED_DATA_EXTENSION;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string ANIMATION_CACHE_SUFFIX;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string ENCRYPTED_ANIMATION_CACHE_SUFFIX;
};
