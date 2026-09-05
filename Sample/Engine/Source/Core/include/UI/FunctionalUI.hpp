#pragma once

#include <CoreMinimal.hpp>

#include <UI/FunctionalBase.hpp>
#include <UI/Image.hpp>
#include <UI/Text.hpp>

BIND_CLASS()
class FunctionalImage : public Image, public FunctionalBase {
public:
    BIND_INIT()
    explicit FunctionalImage(std::shared_ptr<sf::Texture> texture,
                             std::optional<sf::IntRect> rect = std::nullopt);
    virtual ~FunctionalImage() = default;
};

BIND_CLASS()
class FunctionalPlainText : public PlainText, public FunctionalBase {
public:
    BIND_INIT()
    FunctionalPlainText(std::shared_ptr<PlainTextConfig> config,
                        const std::string& text);
    virtual ~FunctionalPlainText() = default;
};

BIND_CLASS()
class FunctionalRichText : public RichText, public FunctionalBase {
public:
    BIND_INIT()
    FunctionalRichText(std::shared_ptr<RichTextConfig> config,
                       const std::string& text);
    virtual ~FunctionalRichText() = default;
};
