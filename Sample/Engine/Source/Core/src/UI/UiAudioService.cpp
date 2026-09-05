#include <UI/UiAudioService.hpp>

namespace {

UiAudioService* activeUiAudioService = nullptr;

}

UiAudioService::~UiAudioService() = default;

void setUiAudioService(UiAudioService* service) {
    activeUiAudioService = service;
}

void playUiSound(const std::string& filename) {
    if (activeUiAudioService != nullptr && !filename.empty()) {
        activeUiAudioService->playSoundEffect(filename);
    }
}
