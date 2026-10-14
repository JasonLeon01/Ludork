#pragma once

#include <Runtime/RuntimeData.hpp>

#include <memory>

namespace ludork::preview_host {

class FrameFiles;

class MaterialPreviewSession {
public:
    MaterialPreviewSession();
    ~MaterialPreviewSession();
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
    void reset();
    static RuntimeData describe();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::preview_host
