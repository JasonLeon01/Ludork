#pragma once

#include <SFML/Audio/Sound.hpp>

#include <AnimSprite.hpp>
#include <Graphics/AnimationSoundEntry.hpp>

#include <CoreMinimal.hpp>

BIND_CLASS(callbacks = true)
class Animation : public AnimSprite {
public:
    LUDORK_CAST_DERIVED(Animation, AnimSprite)

    BIND_CLASS(copyable = true, table_init = true)
    struct AnimationPlayingSound {
        BIND_PROPERTY()
        std::shared_ptr<sf::Sound> sound;

        BIND_PROPERTY()
        int endFrame = -1;
    };

    BIND_INIT(defaults = {false})
    explicit Animation(const AnimSprite::AnimationData& animationData,
                       bool isSpatial = false);

    BIND_METHOD()
    virtual void setData(
        const AnimSprite::AnimationData& animationData) override;

    BIND_METHOD()
    virtual void update(float deltaTime) override;

    BIND_METHOD()
    virtual void playSoundsUpToFrame(int frameIndex);

    BIND_METHOD()
    virtual void stopSoundsAtFrame(int frameIndex);

    BIND_PROPERTY()
    std::vector<AnimationSoundEntry> soundEntries;

    BIND_PROPERTY()
    std::size_t soundIndex = 0;

    BIND_PROPERTY()
    std::vector<Animation::AnimationPlayingSound> playingSounds;

protected:
    const AnimSprite::AnimationData& getAnimationData() const;
    bool getIsSpatial() const;

private:
    AnimSprite::AnimationData animationData_;
    bool isSpatial_ = false;
};
