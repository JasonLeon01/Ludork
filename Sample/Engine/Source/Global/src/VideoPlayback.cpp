#include <VideoPlayback.hpp>

#include "VideoPlaybackImpl.hpp"
#include "VideoPlayerImpl.hpp"

#include <Runtime/AssetPath.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace ludork::video {

VideoPlaybackCompletionScope::VideoPlaybackCompletionScope(
    std::atomic_uint64_t& sequence) noexcept
    : sequence_(sequence) {}

VideoPlaybackCompletionScope::~VideoPlaybackCompletionScope() {
    sequence_.fetch_add(1, std::memory_order_release);
}

}  // namespace ludork::video

using ludork::video::VideoPlaybackCompletionScope;
using ludork::video::VideoPlaybackRequest;

namespace {

std::atomic_uint64_t videoPlaybackCompletionSequence = 0;
std::mutex videoPlaybackMutex;
std::condition_variable videoPlaybackCondition;
std::deque<std::shared_ptr<VideoPlaybackRequest>> pendingVideoPlayback;
std::thread::id videoPlaybackThread;
bool videoPlaybackShuttingDown = false;

void playVideoNow(const std::string& path, bool mute, bool skipable) {
    VideoPlaybackCompletionScope completionScope(
        videoPlaybackCompletionSequence);
    ludork::video::runVideoPlayback(path, mute, skipable);
}

void playVideo(const std::string& path, bool mute, bool skipable) {
    static_cast<void>(ludork::runtime::AssetPath::parse(path));
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
