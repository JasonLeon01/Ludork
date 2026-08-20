#include "UiControlAdapterRegistryBuilder.hpp"

#include "UiControlAdapterSupport.hpp"

#include <UI/FunctionalUI.hpp>
#include <UI/Image.hpp>
#include <UI/ProgressBar.hpp>
#include <UI/Rect.hpp>
#include <UI/SolidRect.hpp>
#include <UI/Window.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

void UiControlAdapterRegistry::Builder::registerVisualAdapters(
    UiControlAdapterRegistry& registry) {
    using namespace ui_control_adapter_detail;

    UiControlAdapterRegistry::Adapter solidRect;
    solidRect.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<SolidRect>(
            vector2fProperty(properties, "size", {100.0f, 32.0f}),
            colorProperty(properties, "fillColor", sf::Color::White),
            colorProperty(properties, "outlineColor", sf::Color::Transparent),
            floatProperty(properties, "outlineThickness", 0.0f));
    };
    solidRect.setter = [](ControlBase& control, const std::string& propertyId,
                          const RuntimeValue& value) {
        SolidRect& rect =
            requireControlType<SolidRect>(control, "Engine.SolidRect");
        if (propertyId == "size") {
            rect.setSize(requireVector2f(value, "size"));
        } else if (propertyId == "fillColor") {
            rect.setFillColor(requireColor(value, "fillColor"));
        } else if (propertyId == "outlineColor") {
            rect.setOutlineColor(requireColor(value, "outlineColor"));
        } else if (propertyId == "outlineThickness") {
            rect.setOutlineThickness(requireFloat(value, "outlineThickness"));
        } else {
            throw std::invalid_argument("Unknown SolidRect property " +
                                        propertyId);
        }
    };
    solidRect.arranger = [](ControlBase& control, const sf::Vector2f& size,
                            const sf::Vector2f& renderScale) {
        SolidRect& rect =
            requireControlType<SolidRect>(control, "Engine.SolidRect");
        rect.setSize({std::max(0.0f, size.x), std::max(0.0f, size.y)});
        rect.setScale(renderScale);
    };
    registry.registerAdapter<SolidRectUiControlAdapterTag>(
        std::move(solidRect));

    UiControlAdapterRegistry::Adapter progressBar;
    progressBar.factory = [](const RuntimeValue::Map& properties) {
        return std::make_shared<ProgressBar>(
            vector2fProperty(properties, "size", {100.0f, 12.0f}),
            floatProperty(properties, "progress", 0.0f),
            colorProperty(properties, "backgroundColor",
                          sf::Color(255, 255, 255, 64)),
            colorProperty(properties, "fillColor", sf::Color::White));
    };
    progressBar.setter = [](ControlBase& control, const std::string& propertyId,
                            const RuntimeValue& value) {
        ProgressBar& progress =
            requireControlType<ProgressBar>(control, "Engine.ProgressBar");
        if (propertyId == "size") {
            progress.resize(requireVector2f(value, "size"));
        } else if (propertyId == "progress") {
            progress.setProgress(requireFloat(value, "progress"));
        } else if (propertyId == "backgroundColor") {
            progress.setBackgroundColor(requireColor(value, "backgroundColor"));
        } else if (propertyId == "fillColor") {
            progress.setFillColor(requireColor(value, "fillColor"));
        } else {
            throw std::invalid_argument("Unknown ProgressBar property " +
                                        propertyId);
        }
    };
    progressBar.arranger = [](ControlBase& control, const sf::Vector2f& size,
                              const sf::Vector2f& renderScale) {
        ProgressBar& progress =
            requireControlType<ProgressBar>(control, "Engine.ProgressBar");
        progress.resize(size);
        progress.setScale(renderScale);
    };
    registry.registerAdapter<ProgressBarUiControlAdapterTag>(
        std::move(progressBar));

    UiControlAdapterRegistry::Adapter image;
    image.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<Image> result = std::make_shared<Image>(
            loadTexture(stringProperty(properties, "texture")),
            optionalIntRectProperty(properties, "textureRect"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    image.setter = [](ControlBase& control, const std::string& propertyId,
                      const RuntimeValue& value) {
        Image& image = requireControlType<Image>(control, "Engine.Image");
        if (propertyId == "texture") {
            image.setTexture(loadTexture(requireString(value, "texture")),
                             true);
        } else if (propertyId == "textureRect") {
            if (value.isNil()) {
                image.setTextureRect(
                    {{0, 0},
                     {static_cast<int>(image.getTexture().getSize().x),
                      static_cast<int>(image.getTexture().getSize().y)}});
            } else {
                image.setTextureRect(requireIntRect(value, "textureRect"));
            }
        } else if (propertyId == "colour") {
            image.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument("Unknown Image property " + propertyId);
        }
    };
    image.arranger = [](ControlBase& control, const sf::Vector2f& size,
                        const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    registry.registerAdapter<ImageUiControlAdapterTag>(std::move(image));

    UiControlAdapterRegistry::Adapter functionalImage;
    functionalImage.factory = [](const RuntimeValue::Map& properties) {
        std::shared_ptr<FunctionalImage> result =
            std::make_shared<FunctionalImage>(
                loadTexture(stringProperty(properties, "texture")),
                optionalIntRectProperty(properties, "textureRect"));
        result->setColour(
            colorProperty(properties, "colour", sf::Color::White));
        return result;
    };
    functionalImage.setter = [](ControlBase& control,
                                const std::string& propertyId,
                                const RuntimeValue& value) {
        FunctionalImage& image = requireControlType<FunctionalImage>(
            control, "Engine.FunctionalImage");
        if (propertyId == "texture") {
            image.setTexture(loadTexture(requireString(value, "texture")),
                             true);
        } else if (propertyId == "textureRect") {
            if (value.isNil()) {
                image.setTextureRect(
                    {{0, 0},
                     {static_cast<int>(image.getTexture().getSize().x),
                      static_cast<int>(image.getTexture().getSize().y)}});
            } else {
                image.setTextureRect(requireIntRect(value, "textureRect"));
            }
        } else if (propertyId == "colour") {
            image.setColour(requireColor(value, "colour"));
        } else {
            throw std::invalid_argument("Unknown FunctionalImage property " +
                                        propertyId);
        }
    };
    functionalImage.arranger = [](ControlBase& control,
                                  const sf::Vector2f& size,
                                  const sf::Vector2f& renderScale) {
        arrangeByScale(control, size, renderScale);
    };
    registry.registerAdapter<FunctionalImageUiControlAdapterTag>(
        std::move(functionalImage));
}

void UiControlAdapterRegistry::Builder::registerSkinnedAdapters(
    UiControlAdapterRegistry& registry) {
    using namespace ui_control_adapter_detail;

    UiControlAdapterRegistry::Adapter window;
    window.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2u size =
            vector2uProperty(properties, "size", {160u, 96u});
        return std::make_shared<Window>(
            sf::IntRect({0, 0},
                        {static_cast<int>(size.x), static_cast<int>(size.y)}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            boolProperty(properties, "repeated", false));
    };
    window.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Window& window = requireControlType<Window>(control, "Engine.Window");
        if (propertyId == "size") {
            window.resize(requireVector2u(value, "size"));
            return;
        }
        throw std::invalid_argument(propertyId +
                                    " is a construction-only Window property");
    };
    window.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        Window& window = requireControlType<Window>(control, "Engine.Window");
        window.resize(
            {static_cast<unsigned int>(std::max(0.0f, std::round(size.x))),
             static_cast<unsigned int>(std::max(0.0f, std::round(size.y)))});
        window.setScale(renderScale);
    };
    registry.registerAdapter<WindowUiControlAdapterTag>(std::move(window));

    UiControlAdapterRegistry::Adapter rect;
    rect.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2f size =
            vector2fProperty(properties, "size", {160.0f, 96.0f});
        const std::string curve = stringProperty(properties, "opacityCurve");
        return std::make_shared<Rect>(
            sf::IntRect({0, 0}, {static_cast<int>(std::round(size.x)),
                                 static_cast<int>(std::round(size.y))}),
            loadWindowSkin(stringProperty(properties, "windowSkin")),
            curve.empty() ? std::nullopt : std::optional<std::string>(curve));
    };
    rect.setter = [](ControlBase& control, const std::string& propertyId,
                     const RuntimeValue& value) {
        Rect& rect = requireControlType<Rect>(control, "Engine.Rect");
        if (propertyId == "size") {
            rect.resize(requireVector2f(value, "size"));
            return;
        }
        throw std::invalid_argument(propertyId +
                                    " is a construction-only Rect property");
    };
    rect.arranger = [](ControlBase& control, const sf::Vector2f& size,
                       const sf::Vector2f& renderScale) {
        Rect& rect = requireControlType<Rect>(control, "Engine.Rect");
        rect.resize(size);
        rect.setScale(renderScale);
    };
    registry.registerAdapter<RectUiControlAdapterTag>(std::move(rect));
}
