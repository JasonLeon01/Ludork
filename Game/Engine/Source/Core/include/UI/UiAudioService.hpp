#pragma once

#include <EngineRuntimeApi.hpp>

#include <string>

class LUDORK_ENGINE_API UiAudioService {
public:
    virtual ~UiAudioService();
    virtual void playSoundEffect(const std::string& filename) = 0;
};

LUDORK_ENGINE_API void setUiAudioService(UiAudioService* service);
LUDORK_ENGINE_API void playUiSound(const std::string& filename);
