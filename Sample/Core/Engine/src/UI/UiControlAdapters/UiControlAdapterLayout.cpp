#include "UiControlAdapterRegistryBuilder.hpp"

#include "UiControlAdapterSupport.hpp"

#include <UI/Canvas.hpp>
#include <UI/ListView.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

void UiControlAdapterRegistry::Builder::registerLayoutAdapters(
    UiControlAdapterRegistry& registry) {
    using namespace ui_control_adapter_detail;

    UiControlAdapterRegistry::Adapter canvas;
    canvas.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2u size =
            vector2uProperty(properties, "size", {100u, 100u});
        return std::make_shared<Canvas>(sf::IntRect(
            {0, 0}, {static_cast<int>(size.x), static_cast<int>(size.y)}));
    };
    canvas.setter = [](ControlBase& control, const std::string& propertyId,
                       const RuntimeValue& value) {
        Canvas& canvas = requireControlType<Canvas>(control, "Engine.Canvas");
        if (propertyId == "size") {
            canvas.resize(requireVector2u(value, "size"));
            return;
        }
        throw std::invalid_argument("Unknown Canvas property " + propertyId);
    };
    canvas.arranger = [](ControlBase& control, const sf::Vector2f& size,
                         const sf::Vector2f& renderScale) {
        Canvas& canvas = requireControlType<Canvas>(control, "Engine.Canvas");
        canvas.resize(
            {static_cast<unsigned int>(std::max(0.0f, std::round(size.x))),
             static_cast<unsigned int>(std::max(0.0f, std::round(size.y)))});
        canvas.setScale(renderScale);
    };
    canvas.childAttacher =
        [](ControlBase& control,
           const std::vector<std::shared_ptr<ControlBase>>& children) {
            Canvas& canvas =
                requireControlType<Canvas>(control, "Engine.Canvas");
            for (const std::shared_ptr<ControlBase>& child : children) {
                canvas.addChild(child);
            }
        };
    registry.registerAdapter<CanvasUiControlAdapterTag>(std::move(canvas));

    UiControlAdapterRegistry::Adapter listView;
    listView.factory = [](const RuntimeValue::Map& properties) {
        const sf::Vector2f size =
            vector2fProperty(properties, "size", {100.0f, 100.0f});
        const int defaultItemHeight =
            intProperty(properties, "defaultItemHeight", 32);
        const bool fixItemHeight =
            boolProperty(properties, "fixItemHeight", false);
        const int columns = intProperty(properties, "columns", 1);
        return std::make_shared<ListView>(
            sf::IntRect({0, 0}, {static_cast<int>(std::round(size.x)),
                                 static_cast<int>(std::round(size.y))}),
            defaultItemHeight, fixItemHeight, columns);
    };
    listView.setter = [](ControlBase& control, const std::string& propertyId,
                         const RuntimeValue& value) {
        ListView& list =
            requireControlType<ListView>(control, "Engine.ListView");
        if (propertyId == "size") {
            list.setSizeVector2f(requireVector2f(value, "size"));
            return;
        }
        if (propertyId == "columns") {
            list.setColumns(requireInt(value, "columns"));
            return;
        }
        throw std::invalid_argument(
            propertyId + " is a construction-only ListView property");
    };
    listView.arranger = [](ControlBase& control, const sf::Vector2f& size,
                           const sf::Vector2f& renderScale) {
        ListView& list =
            requireControlType<ListView>(control, "Engine.ListView");
        list.setSizeVector2f({std::max(0.0f, size.x), std::max(0.0f, size.y)});
        list.setScale(renderScale);
        list.invalidatePositions();
    };
    listView.childAttacher =
        [](ControlBase& control,
           const std::vector<std::shared_ptr<ControlBase>>& children) {
            ListView& list =
                requireControlType<ListView>(control, "Engine.ListView");
            for (const std::shared_ptr<ControlBase>& child : children) {
                list.addChild(child);
            }
        };
    listView.childReflow = [](ControlBase& control) {
        ListView& list =
            requireControlType<ListView>(control, "Engine.ListView");
        list.invalidatePositions();
        list.applyPositions();
    };
    registry.registerAdapter<ListViewUiControlAdapterTag>(std::move(listView));
}
