#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Manager/AudioEffectControl.hpp>
#include <SFML/Audio/Music.hpp>
#include <mutex>

namespace ludork::global::managed_audio_source_impl {
class EffectImpl;
class EffectStateToken;
}  // namespace ludork::global::managed_audio_source_impl

namespace ludork::global::audio {
class ManagedMusic;
class ManagedSound;
}  // namespace ludork::global::audio

namespace ludork::runtime {
class AssetInputStream;
}

namespace ludork::global::audio {

class ManagedAssetStreamOwner {
public:
    ManagedAssetStreamOwner();
    ~ManagedAssetStreamOwner();

protected:
    std::unique_ptr<ludork::runtime::AssetInputStream> assetStream_;
};

class LUDORK_GLOBAL_API ManagedMusic final : private ManagedAssetStreamOwner,
                                             public sf::Music {
public:
    ManagedMusic();
    ~ManagedMusic() override;

    void play() override;
    void pause() override;
    void stop() override;
    void setEffectProcessor(EffectProcessor effectProcessor) override;
    [[nodiscard]] bool openFromAsset(const std::string& assetPath);

    void beginEffectAttachment(
        const std::shared_ptr<::AudioEffectControl>& control);
    void finishEffectAttachment();
    void abortEffectAttachment();
    void notifyNaturalInputEnded() noexcept;
    [[nodiscard]] bool isNaturalInputDrained() const noexcept;
    [[nodiscard]] bool wasExplicitlyStopped() const noexcept;

private:
    std::unique_ptr<managed_audio_source_impl::EffectImpl> effectImpl_;
    std::mutex mutationMutex_;
};

}  // namespace ludork::global::audio
