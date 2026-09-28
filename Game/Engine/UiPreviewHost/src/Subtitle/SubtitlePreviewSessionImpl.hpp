#pragma once

#include "SubtitlePreviewSession.hpp"

#include <UI/SubtitleRenderer.hpp>
#include <UI/SubtitleTrack.hpp>
#include <VideoDecoder.hpp>

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Context.hpp>
#if LUDORK_HAS_FFMPEG
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/Sprite.hpp>
#endif

#include <optional>
#include <string>
#include <unordered_map>

namespace ludork::preview_host {

struct SubtitlePreviewSession::Impl {
    struct Session {
        sf::Context context;
        std::unique_ptr<sf::RenderTexture> target;
        ludork::video::SubtitleTrack track;
        ludork::video::SubtitleRenderer renderer;
        sf::Vector2f logicalSize;
        std::string language = "en_GB";
        sf::Clock clock;
        double position = 0;
        double videoDuration = 0;
        bool playing = false;
        bool mute = false;
#if LUDORK_HAS_FFMPEG
        std::unique_ptr<ludork::video::VideoDecoder> decoder;
        std::optional<sf::Texture> texture;
        std::optional<sf::Sprite> sprite;
        std::optional<sf::SoundBuffer> soundBuffer;
        std::optional<sf::Sound> sound;
#endif
        void loadVideo(const std::string& path);
        void configureTarget(sf::Vector2u size);
        double duration() const;
        double currentTime() const;
        void pause();
        void play();
        void seek(double time);
        void updateVideo(double time, bool seek);
        RuntimeData frame(const std::string& sessionId, std::int64_t generation,
                          FrameFiles& frameFiles);
    };
    std::unordered_map<std::string, std::unique_ptr<Session>> sessions;
    std::unordered_map<std::string, std::int64_t> generations;
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
};

}  // namespace ludork::preview_host
