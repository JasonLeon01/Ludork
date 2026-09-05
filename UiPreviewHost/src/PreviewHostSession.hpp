#pragma once

#include "Actor/ActorBatchRenderer.hpp"
#include "Protocol/FrameFiles.hpp"
#include "UI/UiPreviewSession.hpp"

#include <Runtime/RuntimeData.hpp>

#include <string>
#include <string_view>

namespace ludork::preview_host {

class PreviewHostSession {
public:
    explicit PreviewHostSession(std::string_view adapterFingerprint);
    ~PreviewHostSession() noexcept;

    PreviewHostSession(const PreviewHostSession&) = delete;
    PreviewHostSession& operator=(const PreviewHostSession&) = delete;
    PreviewHostSession(PreviewHostSession&&) = delete;
    PreviewHostSession& operator=(PreviewHostSession&&) = delete;

    RuntimeData handle(const RuntimeData& requestValue);

private:
    RuntimeData handshake(const RuntimeData::Map& request);

    std::string adapterFingerprint_;
    FrameFiles frameFiles_;
    ActorBatchRenderer actorRenderer_;
    UiPreviewSession uiSession_;
    bool accepted_ = false;
};

}  // namespace ludork::preview_host
