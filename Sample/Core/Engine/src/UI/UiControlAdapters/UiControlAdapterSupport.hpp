#pragma once

#include <Runtime/RuntimeValueReader.hpp>
#include <UI/ControlBase.hpp>
#include <UI/Text.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace ui_control_adapter_detail {

using ludork::engine::runtime_value_reader::requireBool;
using ludork::engine::runtime_value_reader::requireFloat;
using ludork::engine::runtime_value_reader::requireInt;
using ludork::engine::runtime_value_reader::requireString;

sf::Vector2f requireVector2f(const RuntimeValue& value,
                             const std::string& source);
sf::Vector2u requireVector2u(const RuntimeValue& value,
                             const std::string& source);
sf::IntRect requireIntRect(const RuntimeValue& value,
                           const std::string& source);
sf::Color requireColor(const RuntimeValue& value, const std::string& source);

sf::Vector2f vector2fProperty(const RuntimeValue::Map& properties,
                              const std::string& name,
                              const sf::Vector2f& fallback);
sf::Vector2u vector2uProperty(const RuntimeValue::Map& properties,
                              const std::string& name,
                              const sf::Vector2u& fallback);
std::string stringProperty(const RuntimeValue::Map& properties,
                           const std::string& name,
                           const std::string& fallback = {});
int intProperty(const RuntimeValue::Map& properties, const std::string& name,
                int fallback);
float floatProperty(const RuntimeValue::Map& properties,
                    const std::string& name, float fallback);
bool boolProperty(const RuntimeValue::Map& properties, const std::string& name,
                  bool fallback);
sf::Color colorProperty(const RuntimeValue::Map& properties,
                        const std::string& name, const sf::Color& fallback);
std::optional<sf::IntRect> optionalIntRectProperty(
    const RuntimeValue::Map& properties, const std::string& name);

std::shared_ptr<sf::Texture> loadTexture(const std::string& assetKey);
sf::Image loadWindowSkin(const std::string& requestedKey);
std::shared_ptr<PlainTextConfig> plainTextConfig(
    const std::string& textConfigKey);
std::shared_ptr<PlainTextConfig> plainTextControlConfig(
    const RuntimeValue::Map& properties);
std::shared_ptr<RichTextConfig> richTextConfig(
    const std::string& textConfigKey);

void arrangeByScale(ControlBase& control, const sf::Vector2f& size,
                    const sf::Vector2f& renderScale);
void arrangeAtIntrinsicSize(ControlBase& control,
                            const sf::Vector2f& renderScale);

template <typename T>
T& requireControlType(ControlBase& control, const std::string& controlId) {
    T* typed = dynamic_cast<T*>(&control);
    if (typed == nullptr) {
        throw std::logic_error("UI adapter received the wrong control type: " +
                               controlId);
    }
    return *typed;
}

}  // namespace ui_control_adapter_detail
