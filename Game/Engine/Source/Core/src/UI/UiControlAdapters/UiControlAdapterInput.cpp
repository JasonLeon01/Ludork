#include "UiControlAdapterRegistryBuilderImpl.hpp"

#include "UiControlAdapterSupport.hpp"

#include <UI/Button.hpp>
#include <Input/JoystickButton.hpp>
#include <UI/CheckBox.hpp>
#include <UI/DropBox.hpp>
#include <UI/GamepadHintBar.hpp>
#include <UI/Slider.hpp>
#include <UI/TabView.hpp>
#include <UI/TextBox.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

std::shared_ptr<sf::Texture> buttonTexture(const std::string& assetKey) {
    if (!assetKey.empty()) {
        return ui_control_adapter_detail::loadTexture(assetKey);
    }
    return std::make_shared<sf::Texture>(
        ui_control_adapter_detail::loadWindowSkin(""), false,
        sf::IntRect({128, 64}, {32, 32}));
}

}  // namespace

void UiControlAdapterRegistry::BuilderImpl::registerInputAdapters(
    UiControlAdapterRegistry& registry) {
    using namespace ui_control_adapter_detail;

    UiControlAdapterRegistry::Adapter textBox;
    textBox.factory = [](const UiControlProperties& properties) {
        const std::string value = stringProperty(properties, "text");
        return std::make_shared<TextBox>(
            vector2fProperty(properties, "size", {240.0f, 40.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextControlConfig(properties),
            value.empty() ? stringProperty(properties, "previewText") : value);
    };
    textBox.setter = [](ControlBase& control, const std::string& propertyId,
                        const UiControlPropertyValue& value) {
        TextBox& field = requireControlType<TextBox>(control, "Engine.TextBox");
        if (propertyId == "size") {
            field.resize(requireVector2f(value, "size"));
        } else if (propertyId == "windowSkin") {
            field.setWindowSkin(
                loadWindowSkin(requireString(value, "windowSkin")));
        } else if (propertyId == "textConfig") {
            field.setTextConfig(
                plainTextConfig(requireString(value, "textConfig")));
        } else if (propertyId == "text" || propertyId == "previewText") {
            field.setString(requireString(value, propertyId));
        } else {
            throw std::invalid_argument(
                propertyId + " is a construction-only TextBox property");
        }
    };
    textBox.arranger = [](ControlBase& control, const sf::Vector2f& size,
                          const sf::Vector2f& renderScale) {
        TextBox& field = requireControlType<TextBox>(control, "Engine.TextBox");
        field.resize(size);
        field.setScale(renderScale);
    };
    textBox.properties.emplace("previewText");
    registry.registerAdapter<TextBoxUiControlAdapterTag>(std::move(textBox));

    UiControlAdapterRegistry::Adapter button;
    button.factory = [](const UiControlProperties& properties) {
        const std::string textureKey = stringProperty(properties, "texture");
        const std::shared_ptr<sf::Texture> texture = buttonTexture(textureKey);
        std::shared_ptr<Button> result = std::make_shared<Button>(
            texture, optionalIntRectProperty(properties, "textureRect"),
            colorProperty(properties, "hoverColour", sf::Color::White),
            colorProperty(properties, "pressedColour", sf::Color::White));
        if (textureKey.empty()) {
            result->setDefaultBackgroundTexture(texture);
        }
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        result->setGamepadLongPress(
            boolProperty(properties, "gamepadLongPress", false));
        result->setGamepadButton(JoystickButton::fromName(
            stringProperty(properties, "gamepadButton")));
        return result;
    };
    button.setter = [](ControlBase& control, const std::string& propertyId,
                       const UiControlPropertyValue& value) {
        Button& button = requireControlType<Button>(control, "Engine.Button");
        if (propertyId == "texture") {
            const std::string textureKey = requireString(value, "texture");
            if (textureKey.empty()) {
                button.setDefaultBackgroundTexture(buttonTexture(textureKey),
                                                   true);
            } else {
                button.setTexture(buttonTexture(textureKey), true);
            }
        } else if (propertyId == "textureRect") {
            if (isNil(value)) {
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
        } else if (propertyId == "gamepadButton") {
            button.setGamepadButton(
                JoystickButton::fromName(requireString(value, propertyId)));
        } else if (propertyId == "gamepadLongPress") {
            button.setGamepadLongPress(requireBool(value, propertyId));
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
    checkBox.factory = [](const UiControlProperties& properties) {
        return std::make_shared<CheckBox>(
            vector2fProperty(properties, "size", {32.0f, 32.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextControlConfig(properties),
            boolProperty(properties, "checked", false));
    };
    checkBox.setter = [](ControlBase& control, const std::string& propertyId,
                         const UiControlPropertyValue& value) {
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
            throw std::invalid_argument(
                propertyId + " is a construction-only CheckBox property");
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
    slider.factory = [](const UiControlProperties& properties) {
        return std::make_shared<Slider>(
            vector2fProperty(properties, "size", {64.0f, 8.0f}),
            loadTexture(stringProperty(properties, "lineTexture")),
            loadTexture(stringProperty(properties, "handleTexture")),
            intProperty(properties, "minValue", 0),
            intProperty(properties, "maxValue", 100),
            intProperty(properties, "value", 0));
    };
    slider.setter = [](ControlBase& control, const std::string& propertyId,
                       const UiControlPropertyValue& value) {
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
    dropBox.factory = [](const UiControlProperties& properties) {
        const std::string preview = stringProperty(properties, "previewText");
        std::vector<std::string> items;
        if (!preview.empty()) {
            items.push_back(preview);
        }
        return std::make_shared<DropBox>(
            vector2fProperty(properties, "size", {200.0f, 32.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextControlConfig(properties), std::move(items), 0, false);
    };
    dropBox.setter = [](ControlBase& control, const std::string& propertyId,
                        const UiControlPropertyValue& value) {
        DropBox& field = requireControlType<DropBox>(control, "Engine.DropBox");
        if (propertyId == "size") {
            field.resize(requireVector2f(value, "size"));
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
        DropBox& field = requireControlType<DropBox>(control, "Engine.DropBox");
        field.resize(size);
        field.setScale(renderScale);
    };
    dropBox.properties.emplace("previewText");
    registry.registerAdapter<DropBoxUiControlAdapterTag>(std::move(dropBox));

    UiControlAdapterRegistry::Adapter tabView;
    tabView.factory = [](const UiControlProperties& properties) {
        std::vector<std::string> items =
            stringArrayProperty(properties, "items", {"#TAB"});
        if (items.empty()) {
            throw std::invalid_argument("TabView items must not be empty");
        }
        return std::make_shared<TabView>(
            vector2fProperty(properties, "size", {100.0f, 32.0f}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            plainTextControlConfig(properties), std::move(items), 0);
    };
    tabView.setter = [](ControlBase& control, const std::string& propertyId,
                        const UiControlPropertyValue& value) {
        TabView& tabs = requireControlType<TabView>(control, "Engine.TabView");
        if (propertyId == "size") {
            tabs.resize(requireVector2f(value, "size"));
        } else if (propertyId == "windowSkin") {
            tabs.setWindowSkin(
                loadWindowSkin(requireString(value, "windowSkin")));
        } else if (propertyId == "textConfig") {
            tabs.setTextConfig(
                plainTextConfig(requireString(value, "textConfig")));
        } else if (propertyId == "items") {
            throw std::invalid_argument(
                "items is a construction-only TabView property");
        } else {
            throw std::invalid_argument("Unknown TabView property " +
                                        propertyId);
        }
    };
    tabView.arranger = [](ControlBase& control, const sf::Vector2f& size,
                          const sf::Vector2f& renderScale) {
        TabView& tabs = requireControlType<TabView>(control, "Engine.TabView");
        tabs.resize(size);
        tabs.setScale(renderScale);
    };
    registry.registerAdapter<TabViewUiControlAdapterTag>(std::move(tabView));

    UiControlAdapterRegistry::Adapter gamepadHintBar;
    gamepadHintBar.factory = [](const UiControlProperties& properties) {
        return std::make_shared<GamepadHintBar>(
            vector2fProperty(properties, "size", {200.0f, 24.0f}),
            plainTextControlConfig(properties));
    };
    gamepadHintBar.setter = [](ControlBase& control,
                               const std::string& propertyId,
                               const UiControlPropertyValue& value) {
        GamepadHintBar& hints = requireControlType<GamepadHintBar>(
            control, "Engine.GamepadHintBar");
        if (propertyId == "size") {
            hints.resize(requireVector2f(value, "size"));
        } else if (propertyId == "textConfig") {
            hints.setTextConfig(
                plainTextConfig(requireString(value, "textConfig")));
        } else {
            throw std::invalid_argument(
                propertyId + " is a construction-only GamepadHintBar property");
        }
    };
    gamepadHintBar.arranger = [](ControlBase& control, const sf::Vector2f& size,
                                 const sf::Vector2f& renderScale) {
        GamepadHintBar& hints = requireControlType<GamepadHintBar>(
            control, "Engine.GamepadHintBar");
        hints.resize(size);
        hints.setScale(renderScale);
    };
    registry.registerAdapter<GamepadHintBarUiControlAdapterTag>(
        std::move(gamepadHintBar));
}
