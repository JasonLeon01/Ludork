#include <Manager/UiAudioBridge.hpp>

#include <Manager/AudioManager.hpp>
#include <UI/UiAudioService.hpp>

namespace {

class GlobalUiAudioService final : public UiAudioService {
public:
    void playSoundEffect(const std::string& filename) override {
        AudioManager::playSound(filename);
    }
};

}  // namespace

void initializeUiAudioBridge() {
    static GlobalUiAudioService service;
    setUiAudioService(&service);
}

void shutdownUiAudioBridge() noexcept {
    setUiAudioService(nullptr);
}
