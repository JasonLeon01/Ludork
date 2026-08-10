#include <UI/FunctionalUI.hpp>

#include <utility>

FunctionalImage::FunctionalImage(std::shared_ptr<sf::Texture> texture,
                                 std::optional<sf::IntRect> rect)
    : Image(std::move(texture), rect), FunctionalBase() {}

FunctionalPlainText::FunctionalPlainText(
    std::shared_ptr<PlainTextConfig> config, const std::string& text)
    : PlainText(std::move(config), text), FunctionalBase() {}

FunctionalRichText::FunctionalRichText(std::shared_ptr<RichTextConfig> config,
                                       const std::string& text)
    : RichText(std::move(config), text), FunctionalBase() {}
