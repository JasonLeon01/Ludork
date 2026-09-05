#pragma once

#include <Runtime/RuntimeData.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <memory>

class UiAssetInstance;

namespace ludork::preview_host {

class FrameFiles;

class UiPreviewSession {
public:
    void reset() noexcept;
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
    RuntimeData hitTest(const RuntimeData::Map& request) const;

private:
    std::shared_ptr<UiAssetInstance> instance_;
    std::int64_t generation_ = 0;
    sf::Vector2u designSize_;
    sf::Vector2u renderSize_;
    float renderScale_ = 1.0f;
};

}  // namespace ludork::preview_host
