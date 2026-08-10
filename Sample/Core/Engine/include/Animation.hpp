#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Graphics/CompressAnimation.hpp>

#include <SFML/Graphics/Sprite.hpp>

#include <cstddef>
#include <memory>
#include <vector>

BIND_CLASS(cast_bases = "sf::Drawable,sf::Transformable",
           callbacks =
               "setData,getDuration,getVisualDuration,isFinished,"
               "getFrameIndex,update,applyFrame")
class LUDORK_ENGINE_API AnimSprite : public sf::Sprite {
public:
    BIND_INIT()
    explicit AnimSprite(const AnimationData& animationData);
    virtual ~AnimSprite() = default;

    BIND_METHOD()
    virtual void setData(const AnimationData& animationData);

    BIND_METHOD(Pure = true)
    virtual float getDuration() const;

    BIND_METHOD(Pure = true)
    virtual float getVisualDuration() const;

    BIND_METHOD(Pure = true)
    std::vector<AnimationTimeTag> getAllTimeTags() const;

    BIND_METHOD(Pure = true)
    virtual bool isFinished() const;

    BIND_METHOD(Pure = true)
    virtual int getFrameIndex() const;

    BIND_METHOD()
    virtual void update(float deltaTime);

    BIND_METHOD()
    virtual void applyFrame(int frameIndex);

protected:
    const std::vector<AnimationSoundEntry>& getSoundEntries() const;
    std::vector<AnimationSoundEntry>& getSoundEntries();

private:
    std::shared_ptr<sf::Texture> texture_;
    std::vector<std::variant<std::string, std::shared_ptr<sf::Image>>> frames_;
    int frameRate_ = 30;
    int frameCount_ = 0;
    float frameCounter_ = 0.0f;
    int frameIndex_ = 0;
    bool finished_ = false;
    float duration_ = 0.0f;
    float visualDuration_ = 0.0f;
    std::string frameEncoding_ = "zlib";
    std::vector<AnimationTimeTag> timeTags_;
    std::vector<AnimationSoundEntry> soundEntries_;
};

BIND_FUNCTION()
float getAnimationVisualDuration(const AnimationSourceData& animationData);

LUDORK_ENGINE_API void shutdownAnimationResources() noexcept;
