#include "UiControlAdapterRegistryBuilder.hpp"

#include "UiControlAdapterSupport.hpp"

#include <UI/FunctionalUI.hpp>
#include <UI/Text.hpp>

#include <memory>
#include <utility>

void UiControlAdapterRegistry::Builder::registerTextAdapters(
    UiControlAdapterRegistry& registry) {
    using namespace ui_control_adapter_detail;

    UiControlAdapterRegistry::Adapter plainText;
    plainText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<PlainText> result =
            std::make_shared<PlainText>(plainTextControlConfig(properties),
                                        stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    plainText.setter = [](ControlBase& control, const std::string& propertyId,
                          const RuntimeValue& value) {
        PlainText& text =
            requireControlType<PlainText>(control, "Engine.PlainText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId + " is a construction-only PlainText property");
        }
    };
    plainText.arranger = [](ControlBase& control, const sf::Vector2f&,
                            const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registry.registerAdapter<PlainTextUiControlAdapterTag>(
        std::move(plainText));

    UiControlAdapterRegistry::Adapter functionalPlainText;
    functionalPlainText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<FunctionalPlainText> result =
            std::make_shared<FunctionalPlainText>(
                plainTextControlConfig(properties),
                stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    functionalPlainText.setter = [](ControlBase& control,
                                    const std::string& propertyId,
                                    const RuntimeValue& value) {
        FunctionalPlainText& text = requireControlType<FunctionalPlainText>(
            control, "Engine.FunctionalPlainText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId +
                " is a construction-only FunctionalPlainText property");
        }
    };
    functionalPlainText.arranger = [](ControlBase& control, const sf::Vector2f&,
                                      const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registry.registerAdapter<FunctionalPlainTextUiControlAdapterTag>(
        std::move(functionalPlainText));

    UiControlAdapterRegistry::Adapter richText;
    richText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<RichText> result = std::make_shared<RichText>(
            richTextConfig(stringProperty(properties, "textConfig")),
            stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    richText.setter = [](ControlBase& control, const std::string& propertyId,
                         const RuntimeValue& value) {
        RichText& text =
            requireControlType<RichText>(control, "Engine.RichText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId + " is a construction-only RichText property");
        }
    };
    richText.arranger = [](ControlBase& control, const sf::Vector2f&,
                           const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registry.registerAdapter<RichTextUiControlAdapterTag>(std::move(richText));

    UiControlAdapterRegistry::Adapter functionalRichText;
    functionalRichText.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<FunctionalRichText> result =
            std::make_shared<FunctionalRichText>(
                richTextConfig(stringProperty(properties, "textConfig")),
                stringProperty(properties, "text"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    functionalRichText.setter = [](ControlBase& control,
                                   const std::string& propertyId,
                                   const RuntimeValue& value) {
        FunctionalRichText& text = requireControlType<FunctionalRichText>(
            control, "Engine.FunctionalRichText");
        if (propertyId == "text") {
            text.setString(requireString(value, "text"));
        } else if (propertyId == "colour") {
            text.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument(
                propertyId +
                " is a construction-only FunctionalRichText property");
        }
    };
    functionalRichText.arranger = [](ControlBase& control, const sf::Vector2f&,
                                     const sf::Vector2f& renderScale) {
        arrangeAtIntrinsicSize(control, renderScale);
    };
    registry.registerAdapter<FunctionalRichTextUiControlAdapterTag>(
        std::move(functionalRichText));
}
