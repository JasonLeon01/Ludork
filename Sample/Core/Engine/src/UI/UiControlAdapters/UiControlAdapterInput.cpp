#include "UiControlAdapterRegistryBuilder.hpp"

#include "UiControlAdapterSupport.hpp"

#include <UI/Button.hpp>
#include <UI/CheckBox.hpp>
#include <UI/DropBox.hpp>
#include <UI/Slider.hpp>

#include <memory>
#include <utility>
#include <vector>

void UiControlAdapterRegistry::Builder::registerInputAdapters(
    UiControlAdapterRegistry& registry) {
    using namespace ui_control_adapter_detail;

    UiControlAdapterRegistry::Adapter button;
    button.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<Button> result = std::make_shared<Button>(
            loadTexture(stringProperty(properties, "texture")),
            optionalIntRectProperty(properties, "textureRect"),
            colorProperty(properties, "hoverColour", sf::Color::White),
            colorProperty(properties, "pressedColour", sf::Color::White));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    button.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Button& button = requireControlType<Button>(control, "Engine.Button");
        if (propertyId == "texture") {
            button.setTexture(loadTexture(requireString(value, "texture")),
                              true);
        } else if (propertyId == "textureRect") {
            if (value.isNil()) {
                button.setTextureRect(
                    {{0, 0},
                     {static_cast<int>(button.getTexture().getSize().x),
                      static_cast<int>(button.getTexture().getSize().y)}});
            } else {
                button.setTextureRect(requireIntRect(value, "textureRect"));
            }
        } else if (propertyId == "colour") {
            button.setColour(requireColor(value, "colour"));
        } else if (propertyId == "hoverColour") {
            button.setHoverColour(requireColor(value, "hoverColour"));
        } else if (propertyId == "pressedColour") {
            button.setPressedColour(requireColor(value, "pressedColour"));
        } else {
            throw std::invalid_argument("Unknown Button property " +
                                        propertyId);
        }
    };
    button.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    registry.registerAdapter<ButtonUiControlAdapterTag>(std::move(button));

    UiControlAdapterRegistry::Adapter checkBox;
    checkBox.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<CheckBox>(
            vector2fProperty(properties, "size", {32.0f, 32.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextConfig(stringProperty(properties, "textConfig")),
            boolProperty(properties, "checked", false));
    };
    checkBox.setter = [](ControlBase& control, const std::string& propertyId,
                         const RuntimeValue& value) {
        CheckBox& check =
            requireControlType<CheckBox>(control, "Engine.CheckBox");
        if (propertyId == "size") {
            check.resize(requireVector2f(value, "size"));
        } else if (propertyId == "checked") {
            check.setChecked(requireBool(value, "checked"));
        } else if (propertyId == "windowSkin") {
            check.setWindowSkin(
                loadWindowSkin(requireString(value, "windowSkin")));
        } else if (propertyId == "textConfig") {
            check.setTextConfig(
                plainTextConfig(requireString(value, "textConfig")));
        } else {
            throw std::invalid_argument("Unknown CheckBox property " +
                                        propertyId);
        }
    };
    checkBox.arranger = [](ControlBase& control, const sf::Vector2f& size,
                           const sf::Vector2f& renderScale) {
        CheckBox& check =
            requireControlType<CheckBox>(control, "Engine.CheckBox");
        check.resize(size);
        check.setScale(renderScale);
    };
    registry.registerAdapter<CheckBoxUiControlAdapterTag>(std::move(checkBox));

    UiControlAdapterRegistry::Adapter slider;
    slider.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<Slider>(
            vector2fProperty(properties, "size", {64.0f, 8.0f}),
            loadTexture(stringProperty(properties, "lineTexture")),
            loadTexture(stringProperty(properties, "handleTexture")),
            intProperty(properties, "minValue", 0),
            intProperty(properties, "maxValue", 100),
            intProperty(properties, "value", 0));
    };
    slider.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Slider& range = requireControlType<Slider>(control, "Engine.Slider");
        if (propertyId == "size") {
            range.resize(requireVector2f(value, "size"));
        } else if (propertyId == "value") {
            range.setValue(requireInt(value, "value"));
        } else if (propertyId == "minValue") {
            const std::pair<int, int> limits = range.getRange();
            range.setRange(requireInt(value, "minValue"), limits.second);
        } else if (propertyId == "maxValue") {
            const std::pair<int, int> limits = range.getRange();
            range.setRange(limits.first, requireInt(value, "maxValue"));
        } else if (propertyId == "lineTexture") {
            range.setLineTexture(
                loadTexture(requireString(value, "lineTexture")));
        } else if (propertyId == "handleTexture") {
            range.setHandleTexture(
                loadTexture(requireString(value, "handleTexture")));
        } else {
            throw std::invalid_argument("Unknown Slider property " +
                                        propertyId);
        }
    };
    slider.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        Slider& range = requireControlType<Slider>(control, "Engine.Slider");
        range.resize(size);
        range.setScale(renderScale);
    };
    registry.registerAdapter<SliderUiControlAdapterTag>(std::move(slider));

    UiControlAdapterRegistry::Adapter dropBox;
    dropBox.factory = [](const RuntimeValue::Map& properties) {
        const std::string preview = stringProperty(properties, "previewText");
        std::vector<std::string> items;
        if (!preview.empty()) {
            items.push_back(preview);
        }
        return std::make_shared<DropBox>(
            vector2fProperty(properties, "size", {200.0f, 32.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextConfig(stringProperty(properties, "textConfig")),
            std::move(items), 0, false);
    };
    dropBox.setter = [](ControlBase& control, const std::string& propertyId,
                        const RuntimeValue& value) {
        DropBox& field = requireControlType<DropBox>(control, "Engine.DropBox");
        if (propertyId == "size") {
            field.setCollapsedSize(requireVector2f(value, "size"));
        } else if (propertyId == "windowSkin") {
            field.setWindowSkin(
                loadWindowSkin(requireString(value, "windowSkin")));
        } else if (propertyId == "textConfig") {
            field.setTextConfig(
                plainTextConfig(requireString(value, "textConfig")));
        } else if (propertyId == "previewText") {
            const std::string& preview = requireString(value, "previewText");
            field.setItems(preview.empty() ? std::vector<std::string>{}
                                           : std::vector<std::string>{preview});
        } else {
            throw std::invalid_argument("Unknown DropBox property " +
                                        propertyId);
        }
    };
    dropBox.arranger = [](ControlBase& control, const sf::Vector2f& size,
                          const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    dropBox.properties.emplace("previewText");
    registry.registerAdapter<DropBoxUiControlAdapterTag>(std::move(dropBox));
}
