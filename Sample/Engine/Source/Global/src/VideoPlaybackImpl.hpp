#pragma once

#include <atomic>
#include <exception>
#include <string>

namespace ludork::video {

struct VideoPlaybackRequest {
    std::string path;
    bool mute = false;
    bool skipable = false;
    bool completed = false;
    std::exception_ptr failure;
};

class VideoPlaybackCompletionScope {
public:
    explicit VideoPlaybackCompletionScope(
        std::atomic_uint64_t& sequence) noexcept;
    ~VideoPlaybackCompletionScope();

    VideoPlaybackCompletionScope(const VideoPlaybackCompletionScope&) = delete;
    VideoPlaybackCompletionScope& operator=(
        const VideoPlaybackCompletionScope&) = delete;

private:
    std::atomic_uint64_t& sequence_;
};

}  // namespace ludork::video
