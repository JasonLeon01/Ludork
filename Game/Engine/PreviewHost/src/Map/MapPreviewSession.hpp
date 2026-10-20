#pragma once

#include <Runtime/RuntimeData.hpp>

#include <memory>
#include <string>

namespace ludork::preview_host {

class FrameFiles;

class MapPreviewSession {
public:
    MapPreviewSession();
    ~MapPreviewSession();
    MapPreviewSession(const MapPreviewSession&) = delete;
    MapPreviewSession& operator=(const MapPreviewSession&) = delete;

    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
    void close(const std::string& sceneKey);
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::preview_host
