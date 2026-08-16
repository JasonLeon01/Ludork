#include <VideoPlayback.hpp>

#include "FFmpegSupport.hpp"
#include "VideoDecoder.hpp"

#include <Input/InputService.hpp>
#include <Manager/TimeManager.hpp>
#include <System.hpp>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace {

std::atomic_uint64_t videoPlaybackCompletionSequence = 0;

class VideoPlaybackCompletionScope {
public:
    ~VideoPlaybackCompletionScope() {
        videoPlaybackCompletionSequence.fetch_add(1, std::memory_order_release);
    }

    VideoPlaybackCompletionScope() = default;
    VideoPlaybackCompletionScope(const VideoPlaybackCompletionScope&) = delete;
    VideoPlaybackCompletionScope& operator=(
        const VideoPlaybackCompletionScope&) = delete;
};

}  // namespace

#if LUDORK_HAS_FFMPEG
namespace {

using ludork::video::CodecContextPtr;
using ludork::video::FormatContextPtr;
using ludork::video::FramePtr;
using ludork::video::openCodec;
using ludork::video::openFormat;
using ludork::video::PacketPtr;
using ludork::video::requireFfmpeg;
using ludork::video::SwrContextPtr;
using ludork::video::VideoDecoder;

struct AudioData {
    std::vector<std::int16_t> samples;
    unsigned int channelCount = 0;
    unsigned int sampleRate = 0;
    std::vector<sf::SoundChannel> channelMap;
};

class WindowFocusRestoreScope {
public:
    explicit WindowFocusRestoreScope(const sf::RenderWindow& window)
        : restore_(window.hasFocus()) {}

    ~WindowFocusRestoreScope() {
        const std::shared_ptr<sf::RenderWindow> window = System::getWindow();
        if (restore_ && window != nullptr && window->isOpen()) {
            window->requestFocus();
        }
    }

    WindowFocusRestoreScope(const WindowFocusRestoreScope&) = delete;
    WindowFocusRestoreScope& operator=(const WindowFocusRestoreScope&) = delete;

private:
    bool restore_;
};

void appendAudioFrame(SwrContext* resampler, AVCodecContext* decoder,
                      AVFrame* frame, AudioData& audio) {
    const int outputCapacity = static_cast<int>(av_rescale_rnd(
        swr_get_delay(resampler, decoder->sample_rate) + frame->nb_samples,
        decoder->sample_rate, decoder->sample_rate, AV_ROUND_UP));
    const std::size_t previousSize = audio.samples.size();
    audio.samples.resize(previousSize +
                         static_cast<std::size_t>(outputCapacity) *
                             audio.channelCount);
    uint8_t* output[] = {
        reinterpret_cast<uint8_t*>(audio.samples.data() + previousSize)};
    const uint8_t* const* input = frame->extended_data;
    const int converted = swr_convert(resampler, output, outputCapacity, input,
                                      frame->nb_samples);
    requireFfmpeg(converted, "Failed to convert video audio");
    audio.samples.resize(previousSize + static_cast<std::size_t>(converted) *
                                            audio.channelCount);
}

void drainAudioDecoder(AVCodecContext* decoder, SwrContext* resampler,
                       AVFrame* frame, AudioData& audio, bool flushing) {
    while (true) {
        const int result = avcodec_receive_frame(decoder, frame);
        if (result == AVERROR(EAGAIN)) {
            if (flushing) {
                throw std::runtime_error(
                    "Audio decoder requested input while flushing");
            }
            return;
        }
        if (result == AVERROR_EOF) {
            return;
        }
        requireFfmpeg(result, "Failed to decode video audio");
        appendAudioFrame(resampler, decoder, frame, audio);
        av_frame_unref(frame);
    }
}

AudioData extractAudio(const std::string& path) {
    FormatContextPtr format = openFormat(path);
    const int streamIndex = av_find_best_stream(
        format.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex == AVERROR_STREAM_NOT_FOUND) {
        return {};
    }
    requireFfmpeg(streamIndex, "Failed to find video audio stream");
    AVStream* stream = format->streams[streamIndex];
    CodecContextPtr decoder = openCodec(stream, AV_CODEC_ID_AAC, "audio");

    AudioData audio;
    audio.channelCount = decoder->ch_layout.nb_channels == 1 ? 1U : 2U;
    audio.sampleRate = static_cast<unsigned int>(decoder->sample_rate);
    audio.channelMap = audio.channelCount == 1
                           ? std::vector{sf::SoundChannel::Mono}
                           : std::vector{sf::SoundChannel::FrontLeft,
                                         sf::SoundChannel::FrontRight};

    AVChannelLayout outputLayout;
    av_channel_layout_default(&outputLayout,
                              static_cast<int>(audio.channelCount));
    SwrContext* rawResampler = nullptr;
    const int allocationResult = swr_alloc_set_opts2(
        &rawResampler, &outputLayout, AV_SAMPLE_FMT_S16, decoder->sample_rate,
        &decoder->ch_layout, decoder->sample_fmt, decoder->sample_rate, 0,
        nullptr);
    av_channel_layout_uninit(&outputLayout);
    SwrContextPtr resampler(rawResampler);
    requireFfmpeg(allocationResult, "Failed to allocate video audio converter");
    requireFfmpeg(swr_init(resampler.get()),
                  "Failed to initialize video audio converter");

    PacketPtr packet(av_packet_alloc());
    FramePtr frame(av_frame_alloc());
    if (packet == nullptr || frame == nullptr) {
        throw std::runtime_error(
            "Failed to allocate video audio decode buffers");
    }
    int readResult = 0;
    while ((readResult = av_read_frame(format.get(), packet.get())) >= 0) {
        if (packet->stream_index == streamIndex) {
            requireFfmpeg(avcodec_send_packet(decoder.get(), packet.get()),
                          "Failed to submit video audio packet");
            drainAudioDecoder(decoder.get(), resampler.get(), frame.get(),
                              audio, false);
        }
        av_packet_unref(packet.get());
    }
    if (readResult != AVERROR_EOF) {
        requireFfmpeg(readResult, "Failed to read video audio packet");
    }
    requireFfmpeg(avcodec_send_packet(decoder.get(), nullptr),
                  "Failed to flush video audio decoder");
    drainAudioDecoder(decoder.get(), resampler.get(), frame.get(), audio, true);

    while (true) {
        const int outputCapacity = swr_get_out_samples(resampler.get(), 0);
        if (outputCapacity <= 0) {
            break;
        }
        const std::size_t previousSize = audio.samples.size();
        audio.samples.resize(previousSize +
                             static_cast<std::size_t>(outputCapacity) *
                                 audio.channelCount);
        uint8_t* output[] = {
            reinterpret_cast<uint8_t*>(audio.samples.data() + previousSize)};
        const int converted =
            swr_convert(resampler.get(), output, outputCapacity, nullptr, 0);
        requireFfmpeg(converted, "Failed to flush video audio converter");
        audio.samples.resize(previousSize +
                             static_cast<std::size_t>(converted) *
                                 audio.channelCount);
        if (converted == 0) {
            break;
        }
    }
    return audio;
}

class VideoPlayback {
public:
    VideoPlayback(std::string path, bool mute, bool skipable)
        : path_(std::move(path)),
          mute_(mute),
          skipable_(skipable),
          decoder_(path_),
          audio_(extractAudio(path_)) {}

    void play() {
        std::shared_ptr<sf::RenderWindow> window = System::getWindow();
        if (window == nullptr) {
            throw std::runtime_error(
                "Video playback requires an active window");
        }
        WindowFocusRestoreScope focusRestoreScope(*window);

        std::optional<sf::SoundBuffer> soundBuffer;
        std::optional<sf::Sound> sound;
        if (!audio_.samples.empty()) {
            soundBuffer.emplace();
            if (!soundBuffer->loadFromSamples(
                    audio_.samples.data(), audio_.samples.size(),
                    audio_.channelCount, audio_.sampleRate,
                    audio_.channelMap)) {
                throw std::runtime_error("Failed to load decoded video audio");
            }
            sound.emplace(*soundBuffer);
            sound->setSpatializationEnabled(false);
            sound->setVolume(mute_ ? 0.0f : 100.0f);
            sound->play();
        }

        sf::Clock silentClock;
        silentClock.restart();
        while (System::isActive()) {
            window = System::getWindow();
            if (window == nullptr) {
                break;
            }
            inputService().update(*window);
            TimeManager::update();
            if (skipable_ && inputService().isActionTriggered(
                                 inputService().getConfirmKeys(), true)) {
                break;
            }
            window->clear(sf::Color::Transparent);
            update(*window, sound, silentClock);
            if (sprite_.has_value()) {
                window->draw(*sprite_);
            }
            System::present();
            window.reset();
            System::completeFrame();
            if (finished_) {
                break;
            }
        }
        if (sound.has_value()) {
            sound->stop();
        }
    }

private:
    void update(sf::RenderWindow& window, const std::optional<sf::Sound>& sound,
                const sf::Clock& silentClock) {
        const float elapsed = sound.has_value()
                                  ? sound->getPlayingOffset().asSeconds()
                                  : silentClock.getElapsedTime().asSeconds();
        const int expectedFrame =
            static_cast<int>(elapsed * static_cast<float>(decoder_.fps()));
        if (sound.has_value() &&
            sound->getStatus() == sf::SoundSource::Status::Stopped &&
            decoder_.frameIndex() > 0) {
            finished_ = true;
            return;
        }
        while (targetFrameIndex_.has_value() &&
               (expectedFrame > *targetFrameIndex_ ||
                (expectedFrame == 0 && !sprite_.has_value()))) {
            targetFrameIndex_ = getFrame(window);
        }
        if (!targetFrameIndex_.has_value()) {
            finished_ = true;
        }
        updateSpriteLayout(window);
    }

    std::optional<int> getFrame(sf::RenderWindow& window) {
        if (!decoder_.readFrame()) {
            sprite_.reset();
            return std::nullopt;
        }
        if (!texture_.has_value()) {
            const sf::Vector2u frameSize{
                static_cast<unsigned int>(decoder_.width()),
                static_cast<unsigned int>(decoder_.height())};
            texture_.emplace(frameSize);
            sprite_.emplace(*texture_);
        }
        texture_->update(decoder_.rgbaFrame().data());
        return decoder_.frameIndex() - 1;
    }

    void updateSpriteLayout(const sf::RenderWindow& window) {
        if (!sprite_.has_value() || !texture_.has_value()) {
            return;
        }
        const sf::Vector2u frameSize = texture_->getSize();
        const sf::View view = window.getView();
        const sf::Vector2f viewSize = view.getSize();
        const float scale =
            std::min(viewSize.x / static_cast<float>(frameSize.x),
                     viewSize.y / static_cast<float>(frameSize.y));
        sprite_->setScale({scale, scale});
        sprite_->setOrigin({static_cast<float>(frameSize.x) / 2.0f,
                            static_cast<float>(frameSize.y) / 2.0f});
        sprite_->setPosition(view.getCenter());
    }

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

void playVideoNow(const std::string& path, bool mute, bool skipable) {
    VideoPlaybackCompletionScope completionScope;
    VideoPlayback(path, mute, skipable).play();
}

}  // namespace
#else
namespace {
void playVideoNow(const std::string&, bool, bool) {
    VideoPlaybackCompletionScope completionScope;
    std::cerr << "Video playback is disabled for this project. Enable FFmpeg "
                 "when creating the project.\n";
}
}  // namespace
#endif

namespace {
struct VideoPlaybackRequest {
    std::string path;
    bool mute = false;
    bool skipable = false;
    bool completed = false;
    std::exception_ptr failure;
};

std::mutex videoPlaybackMutex;
std::condition_variable videoPlaybackCondition;
std::deque<std::shared_ptr<VideoPlaybackRequest>> pendingVideoPlayback;
std::thread::id videoPlaybackThread;
bool videoPlaybackShuttingDown = false;

void playVideo(const std::string& path, bool mute, bool skipable) {
    std::unique_lock<std::mutex> lock(videoPlaybackMutex);
    if (videoPlaybackShuttingDown) {
        throw std::runtime_error("Video playback is shutting down");
    }
    if (videoPlaybackThread == std::thread::id{} ||
        videoPlaybackThread == std::this_thread::get_id()) {
        lock.unlock();
        playVideoNow(path, mute, skipable);
        return;
    }

    const std::shared_ptr<VideoPlaybackRequest> request =
        std::make_shared<VideoPlaybackRequest>(
            VideoPlaybackRequest{path, mute, skipable});
    pendingVideoPlayback.push_back(request);
    videoPlaybackCondition.notify_all();
    videoPlaybackCondition.wait(lock, [&request]() {
        return request->completed || videoPlaybackShuttingDown;
    });
    if (!request->completed) {
        throw std::runtime_error("Video playback stopped during shutdown");
    }
    const std::exception_ptr failure = request->failure;
    lock.unlock();
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
}

int luaPlayVideo(lua_State* state) {
    char error[1024]{};
    {
        try {
            const std::string path = luaL_checkstring(state, 1);
            const bool mute =
                lua_gettop(state) >= 2 && lua_toboolean(state, 2) != 0;
            const bool skipable =
                lua_gettop(state) >= 3 && lua_toboolean(state, 3) != 0;
            playVideo(path, mute, skipable);
        } catch (const std::exception& exception) {
            std::snprintf(error, sizeof(error), "%s", exception.what());
        }
    }
    if (error[0] != '\0') {
        return luaL_error(state, "%s", error);
    }
    return 0;
}
}  // namespace

void registerVideoPlayback(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(videoPlaybackMutex);
        videoPlaybackThread = std::this_thread::get_id();
        videoPlaybackShuttingDown = false;
        pendingVideoPlayback.clear();
    }
    lua_pushcfunction(state, luaPlayVideo);
    lua_setglobal(state, "playVideo");
}

void processPendingVideoPlayback() {
    std::shared_ptr<VideoPlaybackRequest> request;
    {
        const std::lock_guard<std::mutex> lock(videoPlaybackMutex);
        videoPlaybackThread = std::this_thread::get_id();
        if (videoPlaybackShuttingDown || pendingVideoPlayback.empty()) {
            return;
        }
        request = pendingVideoPlayback.front();
        pendingVideoPlayback.pop_front();
    }

    std::exception_ptr failure;
    try {
        playVideoNow(request->path, request->mute, request->skipable);
    } catch (...) {
        failure = std::current_exception();
    }
    {
        const std::lock_guard<std::mutex> lock(videoPlaybackMutex);
        request->failure = failure;
        request->completed = true;
    }
    videoPlaybackCondition.notify_all();
}

std::uint64_t getVideoPlaybackCompletionSequence() noexcept {
    return videoPlaybackCompletionSequence.load(std::memory_order_acquire);
}

void shutdownVideoPlayback() noexcept {
    {
        const std::lock_guard<std::mutex> lock(videoPlaybackMutex);
        videoPlaybackShuttingDown = true;
        videoPlaybackThread = {};
        pendingVideoPlayback.clear();
    }
    videoPlaybackCondition.notify_all();
}
