#include <UI/UiAssetRuntime.hpp>

#include "UiAssetRuntime/AssetBuilder.hpp"
#include "UiAssetRuntime/AnimationRuntime.hpp"
#include "UiAssetRuntime/PathResolver.hpp"
#include "UiAssetRuntime/NodeViewCollector.hpp"
#include "UiAssetRuntime/RuntimeModel.hpp"

#include <Runtime/RuntimeValueReader.hpp>
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

sf::Vector2f requireVector2f(RuntimeValueView value,
                             const std::string& source) {
    RuntimeArrayView array = requireArray(value, source);
    if (array.size() != 2) {
        throw std::invalid_argument(source + " must contain two numbers");
    }
    return {requireFloat(array[0], source + "[0]"),
            requireFloat(array[1], source + "[1]")};
}

bool isProjectControl(const std::string& controlId) {
    return ludork::engine::ui_asset_runtime_impl::isProjectControl(controlId);
}

std::string nestedAssetKey(const std::string& controlId) {
    return ludork::engine::ui_asset_runtime_impl::nestedAssetKey(controlId);
}

sf::Vector2f parseDesignSize(RuntimeMapView asset, const std::string& source) {
    return ludork::engine::ui_asset_runtime_impl::parseDesignSize(asset,
                                                                  source);
}

void requireOnlyKeys(RuntimeMapView values,
                     const std::unordered_set<std::string>& allowed,
                     const std::string& source) {
    for (const auto& [name, value] : values) {
        static_cast<void>(value);
        if (!allowed.contains(name)) {
            throw std::invalid_argument(source + " has unknown field " + name);
        }
    }
}

UiCanvasSlotData parseCanvasSlot(RuntimeValueView value,
                                 const std::string& source) {
    RuntimeMapView slot = requireMap(value, source);
    requireOnlyKeys(slot,
                    {"anchors", "offsets", "alignment", "autoSize", "zOrder"},
                    source);
    UiCanvasSlotData result;
    if (const auto anchors = findValue(slot, "anchors")) {
        RuntimeMapView map = requireMap(*anchors, source + ".anchors");
        requireOnlyKeys(map, {"min", "max"}, source + ".anchors");
        if (const auto minimum = findValue(map, "min")) {
            result.anchorMinimum =
                requireVector2f(*minimum, source + ".anchors.min");
        }
        if (const auto maximum = findValue(map, "max")) {
            result.anchorMaximum =
                requireVector2f(*maximum, source + ".anchors.max");
        }
    }
    if (const auto offsets = findValue(slot, "offsets")) {
        RuntimeMapView map = requireMap(*offsets, source + ".offsets");
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
        result.alignment = requireVector2f(*alignment, source + ".alignment");
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

using AssetLoader = std::function<RuntimeValue(const std::string& assetKey)>;

struct BuildContext {
    const AssetLoader& loader;
    bool designMode = false;
    std::vector<std::string> assetStack;
};

std::string assetReferenceChain(
    const BuildContext& context,
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
    RuntimeValueView value, const std::string& expectedAssetKey,
    BuildContext& context,
    std::optional<sf::Vector2f> logicalSize = std::nullopt);

std::shared_ptr<UiRuntimeNode> buildNode(
    RuntimeValueView value, const std::string& source,
    UiAssetInstanceState& state, BuildContext& context,
    std::unordered_set<std::string>& localNames, bool root);

void attachChildren(const std::shared_ptr<UiRuntimeNode>& node,
                    const std::string& source) {
    if (node->nestedState != nullptr) {
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

void applyCommonProperties(UiRuntimeNode& node, RuntimeMapView properties,
                           const std::string& source) {
    if (const auto visible = findValue(properties, "visible")) {
        node.control->setVisible(requireBool(*visible, source + ".visible"));
    }
    if (const auto rotation = findValue(properties, "rotation")) {
        node.control->setRotationDegrees(
            requireFloat(*rotation, source + ".rotation"));
    }
    if (const auto scale = findValue(properties, "scale")) {
        node.renderScale = requireVector2f(*scale, source + ".scale");
        if (node.renderScale.x < 0.0f || node.renderScale.y < 0.0f) {
            throw std::invalid_argument(source + ".scale cannot be negative");
        }
        node.control->setScale(node.renderScale);
    }
    if (const auto origin = findValue(properties, "origin")) {
        node.control->setOrigin(requireVector2f(*origin, source + ".origin"));
    }
}

RuntimeValue::Map effectiveProperties(RuntimeMapView node,
                                      RuntimeMapView properties,
                                      const std::string& controlId,
                                      bool designMode,
                                      const std::string& source) {
    return ludork::engine::ui_asset_runtime_impl::effectiveProperties(
        node, properties, controlId, designMode, source);
}

std::shared_ptr<UiRuntimeNode> buildNode(
    RuntimeValueView value, const std::string& source,
    UiAssetInstanceState& state, BuildContext& context,
    std::unordered_set<std::string>& localNames, bool root) {
    RuntimeMapView data = requireMap(value, source);
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
                                    " in " + state.assetKey);
    }

    RuntimeMapView storedProperties =
        requireMap(*propertiesValue, source + ".properties");
    RuntimeArrayView children =
        requireArray(*childrenValue, source + ".children");

    const auto slotValue = findValue(data, "slot");
    if (root) {
        if (slotValue.has_value()) {
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
        const RuntimeValue childAsset = [&context, &childAssetKey]() {
            try {
                return context.loader(childAssetKey);
            } catch (const std::exception& exception) {
                throw std::out_of_range(
                    "Unable to load nested UI asset " +
                    assetReferenceChain(context, childAssetKey) + ": " +
                    exception.what());
            }
        }();
        result->nestedState = buildAsset(childAsset, childAssetKey, context);
        result->control = result->nestedState->root->control;
        state.nestedStates.emplace(result->name, result->nestedState);
    } else {
        const UiControlAdapterRegistry& registry =
            UiControlAdapterRegistry::instance();
        if (!registry.contains(result->controlId)) {
            throw std::invalid_argument(source + " has unknown controlId " +
                                        result->controlId);
        }
        RuntimeValue::Map properties =
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
        result->control = registry.create(result->controlId, properties);
        applyCommonProperties(*result, RuntimeMapView(properties),
                              source + ".properties");
        state.controls.emplace(result->name, result);
    }
    result->control->setName(result->name);

    result->children.reserve(children.size());
    for (std::size_t index = 0; index < children.size(); ++index) {
        const std::string childSource =
            source + ".children[" + std::to_string(index) + "]";
        std::shared_ptr<UiRuntimeNode> child = buildNode(
            children[index], childSource, state, context, localNames, false);
        RuntimeMapView childData = requireMap(children[index], childSource);
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
                RuntimeMapView listSlot =
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
    RuntimeValueView value, const std::string& expectedAssetKey,
    BuildContext& context, std::optional<sf::Vector2f> logicalSize) {
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
        RuntimeMapView asset =
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
        RuntimeMapView palette =
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

        std::shared_ptr<UiAssetInstanceState> state =
            std::make_shared<UiAssetInstanceState>();
        state->assetKey = expectedAssetKey;
        state->designSize =
            parseDesignSize(asset, "UI asset " + expectedAssetKey);
        state->logicalSize = logicalSize.value_or(state->designSize);
        std::unordered_set<std::string> localNames;
        state->root = buildNode(*rootValue, expectedAssetKey + ".root", *state,
                                context, localNames, true);

        ludork::engine::ui_asset_runtime_impl::parseAnimations(
            asset, *state, "UI asset " + expectedAssetKey);
        for (const auto& [nodeName, nestedState] : state->nestedStates) {
            nestedState->parentState = state;
            nestedState->parentNodeName = nodeName;
        }
        ludork::engine::ui_asset_runtime_impl::installAnimationUpdater(state);

        UiLayoutEngine::reflow(*state, state->logicalSize);
        context.assetStack.pop_back();
        return state;
    } catch (...) {
        context.assetStack.pop_back();
        throw;
    }
}

void collectControlsByName(const std::shared_ptr<ControlBase>& control,
                           const std::string& name,
                           std::unordered_set<const ControlBase*>& visited,
                           std::vector<std::shared_ptr<ControlBase>>& result) {
    if (control == nullptr || !visited.insert(control.get()).second) {
        return;
    }
    if (control->getName() == name) {
        result.push_back(control);
    }
    for (const std::shared_ptr<ControlBase>& child : control->getChildren()) {
        collectControlsByName(child, name, visited, result);
    }
}

std::shared_ptr<UiAssetInstance> instantiateLoadedAsset(
    const AssetLoader& loader, const std::string& assetKey,
    std::optional<sf::Vector2u> logicalSize, bool designMode) {
    BuildContext context{loader, designMode, {}};
    std::optional<sf::Vector2f> size;
    if (logicalSize.has_value()) {
        if (logicalSize->x == 0 || logicalSize->y == 0) {
            throw std::invalid_argument(
                "UI asset logical size must be positive");
        }
        size = sf::Vector2f{static_cast<float>(logicalSize->x),
                            static_cast<float>(logicalSize->y)};
    }
    const RuntimeValue asset = loader(assetKey);
    return std::shared_ptr<UiAssetInstance>(
        new UiAssetInstance(buildAsset(asset, assetKey, context, size)));
}

}  // namespace

UiAssetInstance::UiAssetInstance(std::shared_ptr<UiAssetInstanceState> state)
    : state_(std::move(state)) {
    if (state_ == nullptr || state_->root == nullptr) {
        throw std::invalid_argument(
            "UI asset instance state must not be empty");
    }
    for (const auto& [name, nestedState] : state_->nestedStates) {
        nestedAssets_.emplace(name, std::shared_ptr<UiAssetInstance>(
                                        new UiAssetInstance(nestedState)));
    }
}

UiAssetInstance::~UiAssetInstance() {
    ludork::engine::ui_asset_runtime_impl::stopAllAnimations(state_);
}

std::shared_ptr<ControlBase> UiAssetInstance::getRoot() const {
    return state_->root->control;
}

std::shared_ptr<ControlBase> UiAssetInstance::requireControl(
    const std::string& localName) const {
    const auto iterator = state_->controls.find(localName);
    if (iterator != state_->controls.end()) {
        return iterator->second->control;
    }
    if (nestedAssets_.contains(localName)) {
        throw std::invalid_argument(
            localName + " is a nested UI asset; use requireAsset instead");
    }
    throw std::out_of_range("UI control not found in " + state_->assetKey +
                            ": " + localName);
}

std::shared_ptr<ControlBase> UiAssetInstance::getNodeByName(
    const std::string& name) const {
    if (name.empty()) {
        throw std::invalid_argument("UI node name cannot be empty");
    }
    std::vector<std::shared_ptr<ControlBase>> matches;
    std::unordered_set<const ControlBase*> visited;
    collectControlsByName(state_->root->control, name, visited, matches);
    if (matches.empty()) {
        return nullptr;
    }
    if (matches.size() != 1) {
        throw std::invalid_argument("UI node name is ambiguous in " +
                                    state_->assetKey + ": " + name);
    }
    return matches.front();
}

std::shared_ptr<UiAssetInstance> UiAssetInstance::requireAsset(
    const std::string& localName) const {
    const auto iterator = nestedAssets_.find(localName);
    if (iterator == nestedAssets_.end()) {
        throw std::out_of_range("Nested UI asset not found in " +
                                state_->assetKey + ": " + localName);
    }
    return iterator->second;
}

void UiAssetInstance::setProperty(const std::string& localName,
                                  const std::string& propertyId,
                                  const RuntimeValue& value) {
    const auto iterator = state_->controls.find(localName);
    if (iterator == state_->controls.end()) {
        if (nestedAssets_.contains(localName)) {
            throw std::invalid_argument(
                "Nested UI asset properties cannot be overridden: " +
                localName);
        }
        throw std::out_of_range("UI control not found in " + state_->assetKey +
                                ": " + localName);
    }
    UiRuntimeNode& node = *iterator->second;
    if (propertyId == "visible") {
        node.control->setVisible(requireBool(value, propertyId));
    } else if (propertyId == "rotation") {
        node.control->setRotationDegrees(requireFloat(value, propertyId));
    } else if (propertyId == "scale") {
        node.renderScale = requireVector2f(value, propertyId);
        if (node.renderScale.x < 0.0f || node.renderScale.y < 0.0f) {
            throw std::invalid_argument("scale cannot be negative");
        }
        node.control->setScale(node.renderScale);
    } else if (propertyId == "origin") {
        node.control->setOrigin(requireVector2f(value, propertyId));
    } else {
        UiControlAdapterRegistry::instance().setProperty(
            node.controlId, *node.control, propertyId, value);
    }
    state_->layoutDirty = true;
}

void UiAssetInstance::setText(const std::string& localName,
                              const std::string& text) {
    const auto iterator = state_->controls.find(localName);
    if (iterator == state_->controls.end()) {
        if (nestedAssets_.contains(localName)) {
            throw std::invalid_argument(
                "Nested UI asset text cannot be overridden: " + localName);
        }
        throw std::out_of_range("UI control not found in " + state_->assetKey +
                                ": " + localName);
    }
    UiRuntimeNode& node = *iterator->second;
    if (!UiControlAdapterRegistry::instance().supportsProperty(node.controlId,
                                                               "text")) {
        throw std::invalid_argument(localName + " is not a text control");
    }
    UiControlAdapterRegistry::instance().setProperty(
        node.controlId, *node.control, "text", RuntimeValue(text));
    state_->layoutDirty = true;
}

void UiAssetInstance::reflow(std::optional<sf::Vector2u> logicalSize) {
    if (logicalSize.has_value()) {
        if (logicalSize->x == 0 || logicalSize->y == 0) {
            throw std::invalid_argument(
                "UI asset logical size must be positive");
        }
        state_->logicalSize = {static_cast<float>(logicalSize->x),
                               static_cast<float>(logicalSize->y)};
    }
    UiLayoutEngine::reflow(*state_, state_->logicalSize);
}

bool UiAssetInstance::hasAnimation(const std::string& name,
                                   std::optional<std::string> target) const {
    return ludork::engine::ui_asset_runtime_impl::hasAnimation(state_, name,
                                                               target);
}

bool UiAssetInstance::playAnimation(const std::string& name,
                                    std::optional<std::string> target,
                                    std::function<void()> onFinished) {
    return ludork::engine::ui_asset_runtime_impl::playAnimation(
        state_, name, target, std::move(onFinished));
}

void UiAssetInstance::stopAnimation(const std::string& name,
                                    std::optional<std::string> target) {
    ludork::engine::ui_asset_runtime_impl::stopAnimation(state_, name, target);
}

bool UiAssetInstance::sampleAnimation(const std::string& name,
                                      std::optional<std::string> target,
                                      float time) {
    return ludork::engine::ui_asset_runtime_impl::sampleAnimation(state_, name,
                                                                  target, time);
}

std::vector<UiAssetNodeView> UiAssetInstance::getNodeViews() const {
    std::vector<UiAssetNodeView> result;
    const auto views =
        ludork::engine::ui_asset_runtime_impl::collectNodeViews(state_->root);
    result.reserve(views.size());
    for (const auto& view : views) {
        result.push_back({view.nodeName, view.control, view.bounds,
                          view.nestedBoundary, view.zOrder, view.drawOrder});
    }
    return result;
}

UiAssetRuntime& UiAssetRuntime::instance() {
    static UiAssetRuntime runtime;
    return runtime;
}

std::shared_ptr<UiAssetInstance> UiAssetRuntime::instantiate(
    const std::string& assetKey,
    std::optional<sf::Vector2u> logicalSize) const {
    static_cast<void>(validateLogicalAssetKey(assetKey));
    AssetLoader loader = [](const std::string& requestedKey) {
        return getJSONData(assetPath(requestedKey));
    };
    return instantiateLoadedAsset(loader, assetKey, logicalSize, false);
}

std::shared_ptr<UiAssetInstance> UiAssetRuntime::instantiateSnapshot(
    const std::string& assetKey, const RuntimeValue& asset,
    const RuntimeValue::Map& dependencies,
    std::optional<sf::Vector2u> logicalSize, bool designMode) const {
    static_cast<void>(validateLogicalAssetKey(assetKey));
    for (const auto& [dependencyKey, dependency] : dependencies) {
        static_cast<void>(dependency);
        static_cast<void>(validateLogicalAssetKey(dependencyKey));
    }
    AssetLoader loader = [&asset, &assetKey, &dependencies](
                             const std::string& requestedKey) -> RuntimeValue {
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
