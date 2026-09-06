#pragma once

namespace ludork::global::audio_manager_impl {

struct AudioImpl;

class CreationScope {
public:
    explicit CreationScope(AudioImpl& impl) noexcept;
    ~CreationScope();
    CreationScope(const CreationScope&) = delete;
    CreationScope& operator=(const CreationScope&) = delete;

    void activate() noexcept;

private:
    AudioImpl* impl_;
    bool active_ = false;
};

}  // namespace ludork::global::audio_manager_impl
