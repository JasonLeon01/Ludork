#pragma once

#include <CoreMinimal.hpp>

#include <Animation.hpp>

#include <SFML/Audio/Sound.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct AnimationPlayingSound {
    BIND_PROPERTY()
    std::shared_ptr<sf::Sound> sound;

    BIND_PROPERTY()
    int endFrame = -1;
};

BIND_CLASS(callbacks = true)
class Animation : public AnimSprite {
public:
    BIND_INIT(defaults = {false})
    explicit Animation(const AnimationData& animationData,
                       bool isSpatial = false);

    BIND_METHOD()
    virtual void setData(const AnimationData& animationData) override;

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
    std::vector<AnimationPlayingSound> playingSounds;

protected:
    const AnimationData& getAnimationData() const;
    bool getIsSpatial() const;

private:
    AnimationData animationData_;
    bool isSpatial_ = false;
};
