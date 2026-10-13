#pragma once

#include <Runtime/RuntimeData.hpp>

#include <memory>

namespace ludork::preview_host {

class FrameFiles;

class ParticlePreviewSession {
public:
    ParticlePreviewSession();
    ~ParticlePreviewSession();

    ParticlePreviewSession(const ParticlePreviewSession&) = delete;
    ParticlePreviewSession& operator=(const ParticlePreviewSession&) = delete;
    ParticlePreviewSession(ParticlePreviewSession&&) = delete;
    ParticlePreviewSession& operator=(ParticlePreviewSession&&) = delete;

    void reset() noexcept;
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::preview_host
