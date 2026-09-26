#pragma once

#include <string>

#if LUDORK_HAS_FFMPEG
#include "VideoAudio.hpp"
#include "VideoDecoder.hpp"

#include <SFML/Audio/Sound.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <optional>
#endif

namespace ludork::video {

void runVideoPlayback(const std::string& path, bool mute, bool skipable);

#if LUDORK_HAS_FFMPEG
class WindowFocusRestoreScope {
public:
    explicit WindowFocusRestoreScope(const sf::RenderWindow& window);
    ~WindowFocusRestoreScope();

    WindowFocusRestoreScope(const WindowFocusRestoreScope&) = delete;
    WindowFocusRestoreScope& operator=(const WindowFocusRestoreScope&) = delete;

private:
    bool restore_;
};

class VideoPlayerImpl {
public:
    VideoPlayerImpl(std::string path, bool mute, bool skipable);

    void play();

private:
    void update(sf::RenderWindow& window, const std::optional<sf::Sound>& sound,
                float elapsed);
    std::optional<int> getFrame(sf::RenderWindow& window);
    void updateSpriteLayout(const sf::RenderWindow& window);

    std::string path_;
    bool mute_ = false;
    bool skipable_ = false;
    VideoDecoder decoder_;
    AudioData audio_;
    std::optional<int> targetFrameIndex_ = 0;
    std::optional<sf::Texture> texture_;
    std::optional<sf::Sprite> sprite_;
    bool finished_ = false;
};
#endif

}  // namespace ludork::video
