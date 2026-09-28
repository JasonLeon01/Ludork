#pragma once

#include <UI/SubtitleRenderer.hpp>
#include <UI/PlainText.hpp>

namespace ludork::video {

struct SubtitleRenderer::Impl {
    std::vector<std::string> lines;
    std::vector<std::unique_ptr<PlainText>> controls;
    std::shared_ptr<sf::Font> font;
    int fontSize = 0;
    float width = 0;
    float height = 0;
    float displayScale = 0;
    void prepare(const std::vector<std::string>& text, float availableWidth);
};

}  // namespace ludork::video
