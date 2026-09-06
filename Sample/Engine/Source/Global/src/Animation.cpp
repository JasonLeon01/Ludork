#include <Animation.hpp>
#include <AnimSprite.hpp>
#include <Graphics/AnimationSoundEntry.hpp>

#include <Filters/SoundFilter.hpp>
#include <Manager/AudioManager.hpp>

#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <utility>

Animation::Animation(const AnimSprite::AnimationData& animationData,
                     bool isSpatial)
    : AnimSprite(animationData),
      animationData_(animationData),
      isSpatial_(isSpatial) {
    soundEntries = getSoundEntries();
    const sf::Vector2u textureSize = getTexture().getSize();
    setOrigin({static_cast<float>(textureSize.x) / 2.0f,
               static_cast<float>(textureSize.y) / 2.0f});
}

void Animation::setData(const AnimSprite::AnimationData& animationData) {
    animationData_ = animationData;
    AnimSprite::setData(animationData);
    soundEntries = getSoundEntries();
    soundIndex = 0;
    playingSounds.clear();
}

void Animation::update(float deltaTime) {
    AnimSprite::update(deltaTime);
    const int frameIndex = getFrameIndex();
    playSoundsUpToFrame(frameIndex);
    stopSoundsAtFrame(frameIndex);
}

void Animation::playSoundsUpToFrame(int frameIndex) {
    while (soundIndex < soundEntries.size()) {
        const AnimationSoundEntry& entry = soundEntries[soundIndex];
        if (entry.startFrame > frameIndex) {
            break;
        }
        if (!entry.asset.empty()) {
            std::shared_ptr<sf::Sound> sound;
            if (isSpatial_) {
                const sf::Vector2f position = getPosition();
                SoundFilter filter;
                filter.spatial = true;
                filter.position = sf::Vector3f{position.x, position.y, 0.0f};
                filter.relativeToListener = false;
                sound = AudioManager::playSound(entry.asset, &filter);
            } else {
                sound = AudioManager::playSound(entry.asset);
            }
            if (sound != nullptr && entry.endFrame >= 0 &&
                entry.stopAtEndFrame.value_or(false)) {
                playingSounds.push_back({std::move(sound), entry.endFrame});
            }
        }
        ++soundIndex;
    }
}

void Animation::stopSoundsAtFrame(int frameIndex) {
    for (Animation::AnimationPlayingSound& entry : playingSounds) {
        if (entry.sound != nullptr && entry.endFrame >= 0 &&
            frameIndex >= entry.endFrame) {
            entry.sound->stop();
        }
    }
    playingSounds.erase(
        std::remove_if(
            playingSounds.begin(), playingSounds.end(),
            [frameIndex](const Animation::AnimationPlayingSound& entry) {
                return entry.endFrame >= 0 && frameIndex >= entry.endFrame;
            }),
        playingSounds.end());
}

const AnimSprite::AnimationData& Animation::getAnimationData() const {
    return animationData_;
}

bool Animation::getIsSpatial() const {
    return isSpatial_;
}
