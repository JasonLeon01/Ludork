#include "SubtitleRendererImpl.hpp"

#include <EngineState.hpp>
#include <UI/PlainTextConfig.hpp>
#include <UI/TextLayout.hpp>
#include <UI/UiResources.hpp>

#include <algorithm>

namespace ludork::video {

SubtitleRenderer::SubtitleRenderer() : impl_(std::make_unique<Impl>()) {}
SubtitleRenderer::~SubtitleRenderer() = default;

void SubtitleRenderer::Impl::prepare(const std::vector<std::string>& text,
                                     float availableWidth) {
    if (lines == text && width == availableWidth &&
        font == uiResources().getDefaultFont() &&
        fontSize == uiResources().getDefaultFontSize() &&
        displayScale == engineState().getScale()) {
        return;
    }
    lines = text;
    width = availableWidth;
    font = uiResources().getDefaultFont();
    fontSize = uiResources().getDefaultFontSize();
    displayScale = engineState().getScale();
    height = 0;
    controls.clear();
    auto config = std::make_shared<PlainTextConfig>();
    config->font = font;
    config->characterSize = static_cast<unsigned int>(std::max(1, fontSize));
    config->outline.thickness =
        std::max(1.0f, static_cast<float>(fontSize) / 12.0f);
    PlainText measure(config, "");
    const float advance = static_cast<float>(config->characterSize) * 1.25f;
    for (const std::string& line : text) {
        const std::string wrapped = wrapPlainText(line, width, measure);
        std::size_t begin = 0;
        do {
            const std::size_t end = wrapped.find('\n', begin);
            controls.push_back(std::make_unique<PlainText>(
                config, wrapped.substr(begin, end - begin)));
            height += advance;
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        } while (begin <= wrapped.size());
    }
}

void SubtitleRenderer::draw(sf::RenderTarget& target,
                            const std::vector<std::string>* lines) {
    if (lines == nullptr || lines->empty()) {
        return;
    }
    const sf::View& view = target.getView();
    const float displayScale = engineState().getScale();
    const sf::Vector2f size = view.getSize() / displayScale;
    const sf::Vector2f center = view.getCenter() / displayScale;
    if (size.x <= 0 || size.y <= 0) {
        return;
    }
    impl_->prepare(*lines, size.x * 0.9f);
    const float scale = std::min(1.0f, size.y * 0.94f / impl_->height);
    const sf::Vector2f topLeft = center - size / 2.0f;
    float y = topLeft.y + size.y * 0.97f - impl_->height * scale;
    for (const auto& control : impl_->controls) {
        const sf::FloatRect bounds = control->getLocalBounds();
        control->setScale({scale, scale});
        control->setPosition(
            {center.x - (bounds.position.x + bounds.size.x / 2.0f) * scale,
             y - bounds.position.y * scale});
        target.draw(*control);
        y += static_cast<float>(std::max(1, impl_->fontSize)) * 1.25f * scale;
    }
}

}  // namespace ludork::video
