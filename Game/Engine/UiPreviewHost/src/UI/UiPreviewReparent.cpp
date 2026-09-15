#include "UI/UiPreviewSession.hpp"

#include "Protocol/PreviewProtocol.hpp"
#include "UI/UiPreviewInstantiation.hpp"

#include <EngineState.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <UI/UiAssetInstance.hpp>
#include <UI/UiControlAdapterRegistry.hpp>

#include <SFML/Window/Context.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::preview_host {
namespace {

RuntimeData::Map& mutableMap(RuntimeData& value) {
    ludork::runtime::value_reader::requireMap(value, "UI reparent data");
    return *value.getMutableIf<RuntimeData::Map>();
}

RuntimeData::Array& mutableChildren(RuntimeData::Map& node) {
    auto iterator = node.find("children");
    if (iterator == node.end()) {
        iterator =
            node.emplace("children", RuntimeData(RuntimeData::Array{})).first;
    }
    ludork::runtime::value_reader::requireArray(iterator->second,
                                                "UI node.children");
    return *iterator->second.getMutableIf<RuntimeData::Array>();
}

std::string nodeString(const RuntimeData::Map& node, const std::string& key) {
    return ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(node, key, "UI node"),
        "UI node." + key);
}

RuntimeData::Map* findNode(RuntimeData& value, const std::string& name) {
    RuntimeData::Map& node = mutableMap(value);
    if (nodeString(node, "name") == name) {
        return &node;
    }
    for (RuntimeData& child : mutableChildren(node)) {
        if (RuntimeData::Map* result = findNode(child, name)) {
            return result;
        }
    }
    return nullptr;
}

std::optional<RuntimeData> detachNode(RuntimeData& value,
                                      const std::string& name,
                                      std::string& parentId) {
    RuntimeData::Map& node = mutableMap(value);
    RuntimeData::Array& children = mutableChildren(node);
    for (auto iterator = children.begin(); iterator != children.end();
         ++iterator) {
        const RuntimeData::Map& child =
            ludork::runtime::value_reader::requireMap(*iterator, "UI node");
        if (nodeString(child, "name") == name) {
            parentId = nodeString(node, "controlId");
            RuntimeData result = std::move(*iterator);
            children.erase(iterator);
            return result;
        }
        if (std::optional<RuntimeData> result =
                detachNode(*iterator, name, parentId)) {
            return result;
        }
    }
    return std::nullopt;
}

UiAssetInstance::UiAssetNodeView nodeView(
    const std::shared_ptr<UiAssetInstance>& instance, const std::string& name) {
    for (const UiAssetInstance::UiAssetNodeView& node :
         instance->getNodeViews()) {
        if (node.nodeName == name) {
            return node;
        }
    }
    throw std::invalid_argument("UI preview node has no layout: " + name);
}

sf::Vector2f visualSize(const ControlBase& control) {
    const sf::Transform transform = control.screenRenderTransform();
    const sf::Vector2f size = control.getLocalBounds().size;
    const sf::Vector2f origin = transform.transformPoint({0.0f, 0.0f});
    return {(transform.transformPoint({size.x, 0.0f}) - origin).length(),
            (transform.transformPoint({0.0f, size.y}) - origin).length()};
}

RuntimeData fixedSlot(const sf::Vector2f& size) {
    return RuntimeData(object({
        {"anchors", RuntimeData(object({
                        {"min", RuntimeData(RuntimeData::Array{number(0.0f),
                                                               number(0.0f)})},
                        {"max", RuntimeData(RuntimeData::Array{number(0.0f),
                                                               number(0.0f)})},
                    }))},
        {"offsets", RuntimeData(object({
                        {"left", number(0.0f)},
                        {"top", number(0.0f)},
                        {"right", number(size.x)},
                        {"bottom", number(size.y)},
                    }))},
        {"alignment",
         RuntimeData(RuntimeData::Array{number(0.0f), number(0.0f)})},
        {"autoSize", RuntimeData(false)},
        {"zOrder", RuntimeData(std::int64_t{0})},
    }));
}

bool autoSized(const RuntimeData::Map& slot) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(slot, "autoSize");
    return value != nullptr &&
           ludork::runtime::value_reader::requireBool(*value, "Slot.autoSize");
}

bool stretched(const RuntimeData::Map& slot, std::size_t axis) {
    const RuntimeData* anchors =
        ludork::runtime::value_reader::findValue(slot, "anchors");
    if (anchors == nullptr || autoSized(slot)) {
        return false;
    }
    const RuntimeData::Map& values =
        ludork::runtime::value_reader::requireMap(*anchors, "Slot.anchors");
    const RuntimeData* minimum =
        ludork::runtime::value_reader::findValue(values, "min");
    const RuntimeData* maximum =
        ludork::runtime::value_reader::findValue(values, "max");
    const float start = minimum == nullptr
                            ? 0.0f
                            : ludork::runtime::value_reader::requireFloat(
                                  ludork::runtime::value_reader::requireArray(
                                      *minimum, "Slot.anchors.min")[axis],
                                  "Slot.anchors.min");
    const float end = maximum == nullptr
                          ? 0.0f
                          : ludork::runtime::value_reader::requireFloat(
                                ludork::runtime::value_reader::requireArray(
                                    *maximum, "Slot.anchors.max")[axis],
                                "Slot.anchors.max");
    return std::abs(end - start) > 0.00001f;
}

RuntimeData::Map& slotOffsets(RuntimeData& slot) {
    RuntimeData::Map& values = mutableMap(slot);
    auto iterator = values.find("offsets");
    if (iterator == values.end()) {
        iterator =
            values.emplace("offsets", RuntimeData(RuntimeData::Map{})).first;
    }
    return mutableMap(iterator->second);
}

float offset(const RuntimeData::Map& offsets, const std::string& name) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(offsets, name);
    return value == nullptr ? 0.0f
                            : ludork::runtime::value_reader::requireFloat(
                                  *value, "Slot.offsets." + name);
}

bool preserveFixedSize(RuntimeData& slot, const ControlBase& before,
                       const ControlBase& after) {
    const RuntimeData::Map& values = mutableMap(slot);
    if (autoSized(values)) {
        return false;
    }
    const sf::Vector2f sourceSize = visualSize(before);
    const sf::Vector2f targetSize = visualSize(after);
    RuntimeData::Map& offsets = slotOffsets(slot);
    bool changed = false;
    const auto preserveAxis = [&](std::size_t axis, const std::string& name,
                                  float source, float target) {
        if (stretched(values, axis) || target <= 0.0f ||
            std::abs(source - target) <= 0.0001f) {
            return;
        }
        const float size = std::max(0.0f, offset(offsets, name));
        const float adjusted = size * (source / target);
        if (!std::isfinite(adjusted)) {
            throw std::invalid_argument("Reparented UI size is not finite");
        }
        offsets[name] = number(adjusted);
        changed |= adjusted != size;
    };
    preserveAxis(0, "right", sourceSize.x, targetSize.x);
    preserveAxis(1, "bottom", sourceSize.y, targetSize.y);
    return changed;
}

void preservePosition(RuntimeData& slot, const sf::Vector2f& before,
                      const ControlBase& after) {
    const std::shared_ptr<ControlBase> parent = after.getParent();
    if (parent == nullptr) {
        throw std::invalid_argument("Reparented UI node has no parent");
    }
    const sf::Transform transform = parent->screenRenderTransform();
    const sf::Vector2f origin = transform.transformPoint({0.0f, 0.0f});
    const sf::Vector2f x = transform.transformPoint({1.0f, 0.0f}) - origin;
    const sf::Vector2f y = transform.transformPoint({0.0f, 1.0f}) - origin;
    if (x.cross(y) == 0.0f) {
        throw std::invalid_argument("UI parent transform is not invertible");
    }
    const sf::Transform inverse = transform.getInverse();
    const sf::Vector2f delta =
        inverse.transformPoint(before) -
        inverse.transformPoint(after.getAbsoluteBounds().position);
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y)) {
        throw std::invalid_argument("Reparented UI position is not finite");
    }
    const RuntimeData::Map& values = mutableMap(slot);
    const bool stretchX = stretched(values, 0);
    const bool stretchY = stretched(values, 1);
    RuntimeData::Map& offsets = slotOffsets(slot);
    offsets["left"] = number(offset(offsets, "left") + delta.x);
    offsets["top"] = number(offset(offsets, "top") + delta.y);
    if (stretchX) {
        offsets["right"] = number(offset(offsets, "right") - delta.x);
    }
    if (stretchY) {
        offsets["bottom"] = number(offset(offsets, "bottom") - delta.y);
    }
}

}  // namespace

RuntimeData UiPreviewSession::resolveReparent(const RuntimeData::Map& request) {
    struct ScaleGuard {
        float value = engineState().getScale();
        ~ScaleGuard() {
            engineState().setScale(value);
        }
    } scaleGuard;
    const std::string assetKey = nodeString(request, "assetKey");
    const std::string nodeName = nodeString(request, "nodeName");
    const std::string parentName = nodeString(request, "parentName");
    const std::int64_t index = ludork::runtime::value_reader::requireInteger(
        ludork::runtime::value_reader::requireValue(request, "index",
                                                    "Reparent request"),
        "Reparent request.index");
    const RuntimeData& asset = ludork::runtime::value_reader::requireValue(
        request, "asset", "Reparent request");
    const RuntimeData::Map& dependencies =
        ludork::runtime::value_reader::requireMap(
            ludork::runtime::value_reader::requireValue(request, "dependencies",
                                                        "Reparent request"),
            "Reparent request.dependencies");
    const sf::Vector2u design = designSize(
        ludork::runtime::value_reader::requireMap(asset, "UI asset"));
    if (context_ == nullptr) {
        context_ = std::make_unique<sf::Context>();
    }
    if (!context_->setActive(true)) {
        throw std::runtime_error("Failed to activate UI preview context");
    }
    const std::shared_ptr<UiAssetInstance> beforeInstance =
        instantiateUiPreview(assetKey, asset, dependencies, design, 1.0f);
    const UiAssetInstance::UiAssetNodeView before =
        nodeView(beforeInstance, nodeName);
    RuntimeData movedAsset = asset;
    RuntimeData& root = mutableMap(movedAsset).at("root");
    std::string sourceParentId;
    std::optional<RuntimeData> node =
        detachNode(root, nodeName, sourceParentId);
    RuntimeData::Map* destination = findNode(root, parentName);
    if (!node.has_value() || destination == nullptr) {
        throw std::invalid_argument(
            "UI reparent source or destination is invalid");
    }
    const UiControlAdapterRegistry& registry =
        UiControlAdapterRegistry::instance();
    const std::string parentId = nodeString(*destination, "controlId");
    if (!registry.contains(parentId) ||
        registry.childPolicy(parentId) == UiChildPolicy::None) {
        throw std::invalid_argument(
            "UI reparent destination cannot have children");
    }
    RuntimeData::Array& children = mutableChildren(*destination);
    if (index < 0 || static_cast<std::uint64_t>(index) > children.size() ||
        (registry.childPolicy(parentId) == UiChildPolicy::Single &&
         !children.empty())) {
        throw std::invalid_argument("UI reparent destination index is invalid");
    }
    RuntimeData& nodeSlot = mutableMap(*node)["slot"];
    if (registry.slotType(parentId) == UiControlSlotType::List) {
        return RuntimeData(object({
            {"type", RuntimeData("reparentSlot")},
            {"slot", RuntimeData(RuntimeData::Map{})},
        }));
    }
    if (registry.slotType(parentId) != UiControlSlotType::Canvas) {
        throw std::invalid_argument(
            "UI reparent destination has no child slots");
    }
    if (registry.slotType(sourceParentId) != UiControlSlotType::Canvas) {
        nodeSlot = fixedSlot(before.control->getSize());
    } else if (nodeSlot.isNil()) {
        nodeSlot = RuntimeData(RuntimeData::Map{});
    }
    RuntimeData slot = nodeSlot;
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(index),
                    std::move(*node));
    std::shared_ptr<UiAssetInstance> afterInstance =
        instantiateUiPreview(assetKey, movedAsset, dependencies, design, 1.0f);
    UiAssetInstance::UiAssetNodeView after = nodeView(afterInstance, nodeName);
    if (preserveFixedSize(slot, *before.control, *after.control)) {
        RuntimeData::Map* movedNode =
            findNode(mutableMap(movedAsset).at("root"), nodeName);
        (*movedNode)["slot"] = slot;
        afterInstance = instantiateUiPreview(assetKey, movedAsset, dependencies,
                                             design, 1.0f);
        after = nodeView(afterInstance, nodeName);
    }
    preservePosition(slot, before.bounds.position, *after.control);
    return RuntimeData(object({
        {"type", RuntimeData("reparentSlot")},
        {"slot", std::move(slot)},
    }));
}

}  // namespace ludork::preview_host
