#pragma once

#include <Runtime/RuntimeData.hpp>

#include <filesystem>
#include <memory>

namespace ludork::preview_host {

class FrameFiles;

class ActorBatchRenderer {
public:
    ActorBatchRenderer();
    ~ActorBatchRenderer();

    ActorBatchRenderer(const ActorBatchRenderer&) = delete;
    ActorBatchRenderer& operator=(const ActorBatchRenderer&) = delete;
    ActorBatchRenderer(ActorBatchRenderer&&) = delete;
    ActorBatchRenderer& operator=(ActorBatchRenderer&&) = delete;

    void reset(const std::filesystem::path& projectPath);
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::preview_host
