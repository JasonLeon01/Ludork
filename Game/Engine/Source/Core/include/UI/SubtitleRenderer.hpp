#pragma once

#include <EngineRuntimeApi.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <memory>
#include <string>
#include <vector>

namespace ludork::video {

class LUDORK_ENGINE_API SubtitleRenderer {
public:
    SubtitleRenderer();
    ~SubtitleRenderer();
    void draw(sf::RenderTarget& target, const std::vector<std::string>* lines);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::video
