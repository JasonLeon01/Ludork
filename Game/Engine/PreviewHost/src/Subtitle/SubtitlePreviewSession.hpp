#pragma once

#include <Runtime/RuntimeData.hpp>

#include <memory>

namespace ludork::preview_host {

class FrameFiles;

class SubtitlePreviewSession {
public:
    SubtitlePreviewSession();
    ~SubtitlePreviewSession();
    void reset() noexcept;
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::preview_host
