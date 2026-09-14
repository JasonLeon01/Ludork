#pragma once

#include <UI/UiControlPropertyValue.hpp>
#include <UI/ControlBase.hpp>
#include <UI/PlainTextConfig.hpp>
#include <UI/RichText.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ui_control_adapter_detail {

template <typename T>
const T& requirePropertyValue(const UiControlPropertyValue& value,
                              const std::string& source) {
    const T* result = std::get_if<T>(&value);
    if (result == nullptr) {
        throw std::invalid_argument(source +
                                    " has an invalid UI property type");
    }
    return *result;
}

bool isNil(const UiControlPropertyValue& value);
bool requireBool(const UiControlPropertyValue& value,
                 const std::string& source);
float requireFloat(const UiControlPropertyValue& value,
                   const std::string& source);
int requireInt(const UiControlPropertyValue& value, const std::string& source);
const std::string& requireString(const UiControlPropertyValue& value,
                                 const std::string& source);
sf::Vector2f requireVector2f(const UiControlPropertyValue& value,
                             const std::string& source);
sf::Vector2u requireVector2u(const UiControlPropertyValue& value,
                             const std::string& source);
sf::IntRect requireIntRect(const UiControlPropertyValue& value,
                           const std::string& source);
sf::Color requireColor(const UiControlPropertyValue& value,
                       const std::string& source);

sf::Vector2f vector2fProperty(const UiControlProperties& properties,
                              const std::string& name,
                              const sf::Vector2f& fallback);
sf::Vector2u vector2uProperty(const UiControlProperties& properties,
                              const std::string& name,
                              const sf::Vector2u& fallback);
std::string stringProperty(const UiControlProperties& properties,
                           const std::string& name,
                           const std::string& fallback = {});
std::vector<std::string> stringArrayProperty(
    const UiControlProperties& properties, const std::string& name,
    const std::vector<std::string>& fallback = {});
int intProperty(const UiControlProperties& properties, const std::string& name,
                int fallback);
float floatProperty(const UiControlProperties& properties,
                    const std::string& name, float fallback);
bool boolProperty(const UiControlProperties& properties,
                  const std::string& name, bool fallback);
sf::Color colorProperty(const UiControlProperties& properties,
                        const std::string& name, const sf::Color& fallback);
std::optional<sf::IntRect> optionalIntRectProperty(
    const UiControlProperties& properties, const std::string& name);

std::shared_ptr<sf::Texture> loadTexture(const std::string& assetKey);
sf::Image loadWindowSkin(const std::string& requestedKey);
std::shared_ptr<PlainTextConfig> plainTextConfig(
    const std::string& textConfigKey);
std::shared_ptr<PlainTextConfig> plainTextControlConfig(
    const UiControlProperties& properties);
std::shared_ptr<RichText::RichTextConfig> richTextConfig(
    const std::string& textConfigKey);

void arrangeByScale(ControlBase& control, const sf::Vector2f& size,
                    const sf::Vector2f& renderScale);
void arrangeAtIntrinsicSize(ControlBase& control,
                            const sf::Vector2f& renderScale);

template <typename T>
T& requireControlType(ControlBase& control, const std::string& controlId) {
    T* typed = ludork::Cast<T>(&control);
    if (typed == nullptr) {
        throw std::logic_error("UI adapter received the wrong control type: " +
                               controlId);
    }
    return *typed;
}

}  // namespace ui_control_adapter_detail
