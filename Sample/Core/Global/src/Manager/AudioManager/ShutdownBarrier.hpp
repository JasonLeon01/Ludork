#pragma once

namespace ludork::global::audio_manager_impl {

struct AudioRuntime;

class CreationScope {
public:
    explicit CreationScope(AudioRuntime& runtime) noexcept;
    ~CreationScope();
    CreationScope(const CreationScope&) = delete;
    CreationScope& operator=(const CreationScope&) = delete;

    void activate() noexcept;

private:
    AudioRuntime* runtime_;
    bool active_ = false;
};

}  // namespace ludork::global::audio_manager_impl
