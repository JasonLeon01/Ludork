#pragma once
#include <Graphics/AnimationTimeTag.hpp>
#include <Graphics/AnimationSoundEntry.hpp>

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

struct AnimationSourceData;

BIND_CLASS(cast_bases = {"sf::Drawable", "sf::Transformable"}, callbacks = true)
class LUDORK_ENGINE_API AnimSprite : public sf::Sprite {
public:
    BIND_CLASS(copyable = true, table_init = true)
    struct AnimationData {
        BIND_PROPERTY()
        std::string type = "compressedAnimation";

        BIND_PROPERTY()
        std::string name;

        BIND_PROPERTY()
        int frameRate = 30;

        BIND_PROPERTY()
        int frameCount = 0;

        BIND_PROPERTY()
        std::optional<int> visualFrameCount;

        BIND_PROPERTY()
        std::optional<float> duration;

        BIND_PROPERTY()
        std::optional<float> visualDuration;

        BIND_PROPERTY()
        std::vector<AnimationTimeTag> timeTags;

        BIND_PROPERTY()
        std::string frameEncoding = "base64+zlib";

        BIND_PROPERTY()
        std::vector<std::variant<std::string, std::shared_ptr<sf::Image>>>
            frames;

        BIND_PROPERTY()
        std::vector<AnimationSoundEntry> sounds;
    };

    BIND_INIT()
    explicit AnimSprite(const AnimSprite::AnimationData& animationData);
    virtual ~AnimSprite() = default;

    BIND_METHOD()
    virtual void setData(const AnimSprite::AnimationData& animationData);

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
