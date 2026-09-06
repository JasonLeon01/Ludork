#include <UI/FunctionalRichText.hpp>
#include <UI/RichText.hpp>

FunctionalRichText::FunctionalRichText(
    std::shared_ptr<RichText::RichTextConfig> config, const std::string& text)
    : RichText(std::move(config), text), FunctionalBase() {}
