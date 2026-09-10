#include <UI/UiAssetRuntime.hpp>
#include <UI/UiAssetInstance.hpp>
#include "UiAssets/ValueReader.hpp"
#include "UiAssetRuntimeImpl.hpp"

#include "UiAssets/AssetBuilder.hpp"
#include "UiAssets/AnimationImpl.hpp"
#include "UiAssets/PathResolver.hpp"
#include "UiAssets/NodeViewCollector.hpp"
#include "UiAssets/AssetImpl.hpp"

#include <Runtime/RuntimeDataReader.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiLayoutEngine.hpp>
#include <Runtime/Json.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace {

using ludork::runtime::value_reader::findValue;
using ludork::runtime::value_reader::requireArray;
using ludork::runtime::value_reader::requireBool;
using ludork::runtime::value_reader::requireFloat;
using ludork::runtime::value_reader::requireInt;
using ludork::runtime::value_reader::requireMap;
using ludork::runtime::value_reader::requireString;

bool isProjectControl(const std::string& controlId) {
    return ludork::engine::ui_asset_runtime_impl::isProjectControl(controlId);
}

std::string nestedAssetKey(const std::string& controlId) {
    return ludork::engine::ui_asset_runtime_impl::nestedAssetKey(controlId);
}

sf::Vector2f parseDesignSize(const RuntimeData::Map& asset,
                             const std::string& source) {
    return ludork::engine::ui_asset_runtime_impl::parseDesignSize(asset,
                                                                  source);
}

void requireOnlyKeys(const RuntimeData::Map& values,
                     const std::unordered_set<std::string>& allowed,
                     const std::string& source) {
    for (const auto& [name, value] : values) {
        static_cast<void>(value);
        if (!allowed.contains(name)) {
            throw std::invalid_argument(source + " has unknown field " + name);
        }
    }
}

UiCanvasSlotData parseCanvasSlot(const RuntimeData& value,
                                 const std::string& source) {
    const RuntimeData::Map& slot = requireMap(value, source);
    requireOnlyKeys(slot,
                    {"anchors", "offsets", "alignment", "autoSize", "zOrder"},
                    source);
    UiCanvasSlotData result;
    if (const auto anchors = findValue(slot, "anchors")) {
        const RuntimeData::Map& map = requireMap(*anchors, source + ".anchors");
        requireOnlyKeys(map, {"min", "max"}, source + ".anchors");
        if (const auto minimum = findValue(map, "min")) {
            result.anchorMinimum =
                ludork::engine::ui_asset_runtime_impl::requireVector2f(
                    *minimum, source + ".anchors.min");
        }
        if (const auto maximum = findValue(map, "max")) {
            result.anchorMaximum =
                ludork::engine::ui_asset_runtime_impl::requireVector2f(
                    *maximum, source + ".anchors.max");
        }
    }
    if (const auto offsets = findValue(slot, "offsets")) {
        const RuntimeData::Map& map = requireMap(*offsets, source + ".offsets");
        requireOnlyKeys(map, {"left", "top", "right", "bottom"},
                        source + ".offsets");
        if (const auto left = findValue(map, "left")) {
            result.offsetLeft = requireFloat(*left, source + ".offsets.left");
        }
        if (const auto top = findValue(map, "top")) {
            result.offsetTop = requireFloat(*top, source + ".offsets.top");
        }
        if (const auto right = findValue(map, "right")) {
            result.offsetRight =
                requireFloat(*right, source + ".offsets.right");
        }
        if (const auto bottom = findValue(map, "bottom")) {
            result.offsetBottom =
                requireFloat(*bottom, source + ".offsets.bottom");
        }
    }
    if (const auto alignment = findValue(slot, "alignment")) {
        result.alignment =
            ludork::engine::ui_asset_runtime_impl::requireVector2f(
                *alignment, source + ".alignment");
    }
    if (const auto autoSize = findValue(slot, "autoSize")) {
        result.autoSize = requireBool(*autoSize, source + ".autoSize");
    }
    if (const auto zOrder = findValue(slot, "zOrder")) {
        result.zOrder = requireInt(*zOrder, source + ".zOrder");
    }

    auto unit = [&](float number, const std::string& field) {
        if (number < 0.0f || number > 1.0f) {
            throw std::invalid_argument(field + " must be between 0 and 1");
        }
    };
    unit(result.anchorMinimum.x, source + ".anchors.min[0]");
    unit(result.anchorMinimum.y, source + ".anchors.min[1]");
    unit(result.anchorMaximum.x, source + ".anchors.max[0]");
    unit(result.anchorMaximum.y, source + ".anchors.max[1]");
    unit(result.alignment.x, source + ".alignment[0]");
    unit(result.alignment.y, source + ".alignment[1]");
    if (result.anchorMinimum.x > result.anchorMaximum.x ||
        result.anchorMinimum.y > result.anchorMaximum.y) {
        throw std::invalid_argument(
            source + " anchor minimum must not exceed anchor maximum");
    }
    return result;
}

using ludork::engine::ui_asset_runtime_impl::assetPath;
using ludork::engine::ui_asset_runtime_impl::validateLogicalAssetKey;

std::string assetReferenceChain(
    const ludork::engine::ui_asset_runtime_impl::BuildContext& context,
    const std::optional<std::string>& target = std::nullopt) {
    std::string result;
    for (const std::string& assetKey : context.assetStack) {
        if (!result.empty()) {
            result += " -> ";
        }
        result += assetKey;
    }
    if (target.has_value()) {
        if (!result.empty()) {
            result += " -> ";
        }
        result += *target;
    }
    return result;
}

std::shared_ptr<UiAssetInstanceState> buildAsset(
    const RuntimeData& value, const std::string& expectedAssetKey,
    ludork::engine::ui_asset_runtime_impl::BuildContext& context,
    std::optional<sf::Vector2f> logicalSize = std::nullopt);

std::shared_ptr<UiRuntimeNode> buildNode(
    const RuntimeData& value, const std::string& source,
    UiAssetInstanceState& impl,
    ludork::engine::ui_asset_runtime_impl::BuildContext& context,
    std::unordered_set<std::string>& localNames, bool root);

void attachChildren(const std::shared_ptr<UiRuntimeNode>& node,
                    const std::string& source) {
    if (node->nestedImpl != nullptr) {
        if (!node->children.empty()) {
            throw std::invalid_argument(
                source + " nested asset cannot have inline children");
        }
        return;
    }
    const UiControlAdapterRegistry& registry =
        UiControlAdapterRegistry::instance();
    const UiChildPolicy policy = registry.childPolicy(node->controlId);
    if (policy == UiChildPolicy::None) {
        if (!node->children.empty()) {
            throw std::invalid_argument(source +
                                        " control does not accept children");
        }
        return;
    }
    if (policy == UiChildPolicy::Single && node->children.size() > 1) {
        throw std::invalid_argument(source + " control accepts only one child");
    }

    std::vector<std::shared_ptr<UiRuntimeNode>> ordered = node->children;
    if (registry.slotType(node->controlId) == UiControlSlotType::Canvas) {
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const std::shared_ptr<UiRuntimeNode>& left,
                            const std::shared_ptr<UiRuntimeNode>& right) {
                             return left->canvasSlot.zOrder <
                                    right->canvasSlot.zOrder;
                         });
    }
    std::vector<std::shared_ptr<ControlBase>> controls;
    controls.reserve(ordered.size());
    for (const std::shared_ptr<UiRuntimeNode>& child : ordered) {
        controls.push_back(child->control);
    }
    registry.attachChildren(node->controlId, *node->control, controls);
}

void applyCommonProperties(UiRuntimeNode& node,
                           const RuntimeData::Map& properties,
                           const std::string& source) {
    if (const auto visible = findValue(properties, "visible")) {
        node.control->setVisible(requireBool(*visible, source + ".visible"));
    }
    if (const auto rotation = findValue(properties, "rotation")) {
        node.control->setRotationDegrees(
            requireFloat(*rotation, source + ".rotation"));
    }
    if (const auto scale = findValue(properties, "scale")) {
        node.renderScale =
            ludork::engine::ui_asset_runtime_impl::requireVector2f(
                *scale, source + ".scale");
        if (node.renderScale.x < 0.0f || node.renderScale.y < 0.0f) {
            throw std::invalid_argument(source + ".scale cannot be negative");
        }
        node.control->setScale(node.renderScale);
    }
    if (const auto origin = findValue(properties, "origin")) {
        node.control->setOrigin(
            ludork::engine::ui_asset_runtime_impl::requireVector2f(
                *origin, source + ".origin"));
    }
}

RuntimeData::Map effectiveProperties(const RuntimeData::Map& node,
                                     const RuntimeData::Map& properties,
                                     const std::string& controlId,
                                     bool designMode,
                                     const std::string& source) {
    return ludork::engine::ui_asset_runtime_impl::effectiveProperties(
        node, properties, controlId, designMode, source);
}

std::shared_ptr<UiRuntimeNode> buildNode(
    const RuntimeData& value, const std::string& source,
    UiAssetInstanceState& impl,
    ludork::engine::ui_asset_runtime_impl::BuildContext& context,
    std::unordered_set<std::string>& localNames, bool root) {
    const RuntimeData::Map& data = requireMap(value, source);
    requireOnlyKeys(
        data, {"name", "controlId", "properties", "slot", "editor", "children"},
        source);
    const auto nameValue = findValue(data, "name");
    const auto controlIdValue = findValue(data, "controlId");
    const auto propertiesValue = findValue(data, "properties");
    const auto childrenValue = findValue(data, "children");
    if (!nameValue || !controlIdValue || !propertiesValue || !childrenValue) {
        throw std::invalid_argument(
            source + " requires name, controlId, properties, and children");
    }

    std::shared_ptr<UiRuntimeNode> result = std::make_shared<UiRuntimeNode>();
    result->name = requireString(*nameValue, source + ".name");
    result->controlId = requireString(*controlIdValue, source + ".controlId");
    if (result->name.empty() || result->controlId.empty()) {
        throw std::invalid_argument(source +
                                    " name and controlId cannot be empty");
    }
    if (!localNames.insert(result->name).second) {
        throw std::invalid_argument("Duplicate UI node name " + result->name +
                                    " in " + impl.assetKey);
    }

    const RuntimeData::Map& storedProperties =
        requireMap(*propertiesValue, source + ".properties");
    const RuntimeData::Array& children =
        requireArray(*childrenValue, source + ".children");

    const auto slotValue = findValue(data, "slot");
    if (root) {
        if (slotValue != nullptr) {
            throw std::invalid_argument(source +
                                        " root node cannot have a slot");
        }
    } else if (!slotValue) {
        throw std::invalid_argument(source + " is missing its parent slot");
    }

    if (isProjectControl(result->controlId)) {
        if (!storedProperties.empty() || !children.empty()) {
            throw std::invalid_argument(
                source + " nested asset properties and children must be empty");
        }
        const std::string childAssetKey = nestedAssetKey(result->controlId);
        static_cast<void>(validateLogicalAssetKey(childAssetKey));
        const RuntimeData childAsset = [&context, &childAssetKey]() {
            try {
                return context.loader(childAssetKey);
            } catch (const std::exception& exception) {
                throw std::out_of_range(
                    "Unable to load nested UI asset " +
                    assetReferenceChain(context, childAssetKey) + ": " +
                    exception.what());
            }
        }();
        result->nestedImpl = buildAsset(childAsset, childAssetKey, context);
        result->control = result->nestedImpl->root->control;
        impl.nestedImpls.emplace(result->name, result->nestedImpl);
    } else {
        const UiControlAdapterRegistry& registry =
            UiControlAdapterRegistry::instance();
        if (!registry.contains(result->controlId)) {
            throw std::invalid_argument(source + " has unknown controlId " +
                                        result->controlId);
        }
        RuntimeData::Map properties =
            effectiveProperties(data, storedProperties, result->controlId,
                                context.designMode, source);
        static const std::unordered_set<std::string> commonProperties = {
            "visible", "rotation", "scale", "origin"};
        for (const auto& [propertyId, property] : properties) {
            static_cast<void>(property);
            if (!commonProperties.contains(propertyId) &&
                !registry.supportsProperty(result->controlId, propertyId)) {
                throw std::invalid_argument(
                    source + ".properties has unknown property " + propertyId +
                    " for " + result->controlId);
            }
        }
        result->control = registry.create(
            result->controlId,
            registry.parseProperties(result->controlId, properties,
                                     source + ".properties"));
        applyCommonProperties(*result, properties, source + ".properties");
        impl.controls.emplace(result->name, result);
    }
    result->control->setName(result->name);

    result->children.reserve(children.size());
    for (std::size_t index = 0; index < children.size(); ++index) {
        const std::string childSource =
            source + ".children[" + std::to_string(index) + "]";
        std::shared_ptr<UiRuntimeNode> child = buildNode(
            children[index], childSource, impl, context, localNames, false);
        const RuntimeData::Map& childData =
            requireMap(children[index], childSource);
        const auto childSlot = findValue(childData, "slot");
        if (!childSlot) {
            throw std::invalid_argument(childSource +
                                        " is missing its parent slot");
        }
        switch (
            UiControlAdapterRegistry::instance().slotType(result->controlId)) {
            case UiControlSlotType::Canvas:
                child->canvasSlot =
                    parseCanvasSlot(*childSlot, childSource + ".slot");
                break;
            case UiControlSlotType::List: {
                const RuntimeData::Map& listSlot =
                    requireMap(*childSlot, childSource + ".slot");
                if (!listSlot.empty()) {
                    throw std::invalid_argument(
                        childSource + ".slot must be empty under a List Slot");
                }
                break;
            }
            case UiControlSlotType::None:
                throw std::invalid_argument(
                    source + " control does not accept children");
        }
        result->children.push_back(std::move(child));
    }
    attachChildren(result, source);
    return result;
}

std::shared_ptr<UiAssetInstanceState> buildAsset(
    const RuntimeData& value, const std::string& expectedAssetKey,
    ludork::engine::ui_asset_runtime_impl::BuildContext& context,
    std::optional<sf::Vector2f> logicalSize) {
    static_cast<void>(validateLogicalAssetKey(expectedAssetKey));
    const bool nested = !context.assetStack.empty();
    if (std::find(context.assetStack.begin(), context.assetStack.end(),
                  expectedAssetKey) != context.assetStack.end()) {
        throw std::invalid_argument(
            "Cyclic UI asset reference: " +
            assetReferenceChain(context, expectedAssetKey));
    }
    context.assetStack.push_back(expectedAssetKey);
    try {
        const RuntimeData::Map& asset =
            requireMap(value, "UI asset " + expectedAssetKey);
        requireOnlyKeys(asset,
                        {"type", "designSize", "palette", "root", "animations"},
                        "UI asset " + expectedAssetKey);
        const auto type = findValue(asset, "type");
        const auto paletteValue = findValue(asset, "palette");
        const auto rootValue = findValue(asset, "root");
        if (!type ||
            requireString(*type, expectedAssetKey + ".type") != "uiAsset") {
            throw std::invalid_argument(expectedAssetKey +
                                        " must be a uiAsset");
        }
        if (!paletteValue) {
            throw std::invalid_argument(expectedAssetKey +
                                        " is missing palette");
        }
        const RuntimeData::Map& palette =
            requireMap(*paletteValue, expectedAssetKey + ".palette");
        if (nested) {
            const auto exposed = findValue(palette, "exposed");
            if (!exposed ||
                !requireBool(*exposed, expectedAssetKey + ".palette.exposed")) {
                throw std::invalid_argument(
                    "Nested UI asset must be exposed: " +
                    assetReferenceChain(context));
            }
        }
        if (!rootValue) {
            throw std::invalid_argument(expectedAssetKey + " is missing root");
        }

        std::shared_ptr<UiAssetInstanceState> impl =
            std::make_shared<UiAssetInstanceState>();
        impl->assetKey = expectedAssetKey;
        impl->designSize =
            parseDesignSize(asset, "UI asset " + expectedAssetKey);
        impl->logicalSize = logicalSize.value_or(impl->designSize);
        std::unordered_set<std::string> localNames;
        impl->root = buildNode(*rootValue, expectedAssetKey + ".root", *impl,
                               context, localNames, true);

        ludork::engine::ui_asset_runtime_impl::parseAnimations(
            asset, *impl, "UI asset " + expectedAssetKey);
        for (const auto& [nodeName, nestedImpl] : impl->nestedImpls) {
            nestedImpl->parentImpl = impl;
            nestedImpl->parentNodeName = nodeName;
        }
        ludork::engine::ui_asset_runtime_impl::installAnimationUpdater(impl);

        UiLayoutEngine::reflow(*impl, impl->logicalSize);
        context.assetStack.pop_back();
        return impl;
    } catch (...) {
        context.assetStack.pop_back();
        throw;
    }
}

std::shared_ptr<UiAssetInstance> instantiateLoadedAsset(
    const ludork::engine::ui_asset_runtime_impl::AssetLoader& loader,
    const std::string& assetKey, std::optional<sf::Vector2u> logicalSize,
    bool designMode) {
    ludork::engine::ui_asset_runtime_impl::BuildContext context{
        loader, designMode, {}};
    std::optional<sf::Vector2f> size;
    if (logicalSize.has_value()) {
        if (logicalSize->x == 0 || logicalSize->y == 0) {
            throw std::invalid_argument(
                "UI asset logical size must be positive");
        }
        size = sf::Vector2f{static_cast<float>(logicalSize->x),
                            static_cast<float>(logicalSize->y)};
    }
    const RuntimeData asset = loader(assetKey);
    return std::shared_ptr<UiAssetInstance>(
        new UiAssetInstance(buildAsset(asset, assetKey, context, size)));
}

}  // namespace

UiAssetRuntime& UiAssetRuntime::instance() {
    static UiAssetRuntime runtime;
    return runtime;
}

std::shared_ptr<UiAssetInstance> UiAssetRuntime::instantiate(
    const std::string& assetKey,
    std::optional<sf::Vector2u> logicalSize) const {
    static_cast<void>(validateLogicalAssetKey(assetKey));
    ludork::engine::ui_asset_runtime_impl::AssetLoader loader =
        [](const std::string& requestedKey) {
            return getJSONData(assetPath(requestedKey));
        };
    return instantiateLoadedAsset(loader, assetKey, logicalSize, false);
}

std::shared_ptr<UiAssetInstance> UiAssetRuntime::instantiateSnapshot(
    const std::string& assetKey, const RuntimeData& asset,
    const RuntimeData::Map& dependencies,
    std::optional<sf::Vector2u> logicalSize, bool designMode) const {
    static_cast<void>(validateLogicalAssetKey(assetKey));
    for (const auto& [dependencyKey, dependency] : dependencies) {
        static_cast<void>(dependency);
        static_cast<void>(validateLogicalAssetKey(dependencyKey));
    }
    ludork::engine::ui_asset_runtime_impl::AssetLoader loader =
        [&asset, &assetKey,
         &dependencies](const std::string& requestedKey) -> RuntimeData {
        if (requestedKey == assetKey) {
            return asset;
        }
        const auto dependency = dependencies.find(requestedKey);
        if (dependency == dependencies.end()) {
            throw std::out_of_range("UI asset snapshot dependency not found: " +
                                    requestedKey);
        }
        return dependency->second;
    };
    return instantiateLoadedAsset(loader, assetKey, logicalSize, designMode);
}

std::shared_ptr<UiAssetInstance> instantiateUiAsset(
    const std::string& assetKey, std::optional<sf::Vector2u> logicalSize) {
    return UiAssetRuntime::instance().instantiate(assetKey, logicalSize);
}
