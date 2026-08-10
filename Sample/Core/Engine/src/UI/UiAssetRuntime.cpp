#include <UI/UiAssetRuntime.hpp>

#include "UiAssetRuntimeInternal.hpp"

#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiLayoutEngine.hpp>
#include <Utils/File.hpp>

#include <Utf8Path.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace {

constexpr const char* projectControlPrefix = "Project:";

const RuntimeValue* findValue(const RuntimeValue::Map& values,
                              const std::string& name) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? nullptr : &iterator->second;
}

const RuntimeValue::Map& requireMap(const RuntimeValue& value,
                                    const std::string& source) {
    const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>();
    if (map == nullptr) {
        throw std::invalid_argument(source + " must be an object");
    }
    return *map;
}

const RuntimeValue::Array& requireArray(const RuntimeValue& value,
                                        const std::string& source) {
    const RuntimeValue::Array* array = value.getIf<RuntimeValue::Array>();
    if (array == nullptr) {
        throw std::invalid_argument(source + " must be an array");
    }
    return *array;
}

const std::string& requireString(const RuntimeValue& value,
                                 const std::string& source) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        throw std::invalid_argument(source + " must be a string");
    }
    return *text;
}

bool requireBool(const RuntimeValue& value, const std::string& source) {
    const bool* boolean = value.getIf<bool>();
    if (boolean == nullptr) {
        throw std::invalid_argument(source + " must be a boolean");
    }
    return *boolean;
}

double requireNumber(const RuntimeValue& value, const std::string& source) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        if (std::isfinite(*number)) {
            return *number;
        }
    }
    throw std::invalid_argument(source + " must be a finite number");
}

float requireFloat(const RuntimeValue& value, const std::string& source) {
    const double number = requireNumber(value, source);
    if (number < -static_cast<double>(std::numeric_limits<float>::max()) ||
        number > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument(source + " is outside the float range");
    }
    return static_cast<float>(number);
}

int requireInt(const RuntimeValue& value, const std::string& source) {
    const std::int64_t* integer = value.getIf<std::int64_t>();
    if (integer == nullptr || *integer < std::numeric_limits<int>::min() ||
        *integer > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(source + " must be an integer");
    }
    return static_cast<int>(*integer);
}

sf::Vector2f requireVector2f(const RuntimeValue& value,
                             const std::string& source) {
    const RuntimeValue::Array& array = requireArray(value, source);
    if (array.size() != 2) {
        throw std::invalid_argument(source + " must contain two numbers");
    }
    return {requireFloat(array[0], source + "[0]"),
            requireFloat(array[1], source + "[1]")};
}

bool isProjectControl(const std::string& controlId) {
    return controlId.starts_with(projectControlPrefix);
}

bool isUuidReference(const std::string& value) {
    if (value.size() != 36) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (value[index] != '-') {
                return false;
            }
        } else if (!std::isxdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

std::string nestedAssetKey(const std::string& controlId) {
    if (!isProjectControl(controlId) ||
        controlId.size() ==
            std::char_traits<char>::length(projectControlPrefix)) {
        throw std::invalid_argument("Invalid project UI control id: " +
                                    controlId);
    }
    const std::string result =
        controlId.substr(std::char_traits<char>::length(projectControlPrefix));
    if (isUuidReference(result)) {
        throw std::invalid_argument(
            "Project UI control id must use a relative asset key, not a "
            "UUID: " +
            controlId);
    }
    return result;
}

sf::Vector2f parseDesignSize(const RuntimeValue::Map& asset,
                             const std::string& source) {
    const RuntimeValue* value = findValue(asset, "designSize");
    if (value == nullptr) {
        throw std::invalid_argument(source + " is missing designSize");
    }
    const RuntimeValue::Map& size = requireMap(*value, source + ".designSize");
    const RuntimeValue* width = findValue(size, "width");
    const RuntimeValue* height = findValue(size, "height");
    if (width == nullptr || height == nullptr) {
        throw std::invalid_argument(source +
                                    ".designSize requires width and height");
    }
    const sf::Vector2f result{
        requireFloat(*width, source + ".designSize.width"),
        requireFloat(*height, source + ".designSize.height")};
    if (result.x <= 0.0f || result.y <= 0.0f) {
        throw std::invalid_argument(source + ".designSize must be positive");
    }
    return result;
}

void requireOnlyKeys(const RuntimeValue::Map& values,
                     const std::unordered_set<std::string>& allowed,
                     const std::string& source) {
    for (const auto& [name, value] : values) {
        static_cast<void>(value);
        if (!allowed.contains(name)) {
            throw std::invalid_argument(source + " has unknown field " + name);
        }
    }
}

UiCanvasSlotData parseCanvasSlot(const RuntimeValue& value,
                                 const std::string& source) {
    const RuntimeValue::Map& slot = requireMap(value, source);
    requireOnlyKeys(slot,
                    {"anchors", "offsets", "alignment", "autoSize", "zOrder"},
                    source);
    UiCanvasSlotData result;
    if (const RuntimeValue* anchors = findValue(slot, "anchors")) {
        const RuntimeValue::Map& map =
            requireMap(*anchors, source + ".anchors");
        requireOnlyKeys(map, {"min", "max"}, source + ".anchors");
        if (const RuntimeValue* minimum = findValue(map, "min")) {
            result.anchorMinimum =
                requireVector2f(*minimum, source + ".anchors.min");
        }
        if (const RuntimeValue* maximum = findValue(map, "max")) {
            result.anchorMaximum =
                requireVector2f(*maximum, source + ".anchors.max");
        }
    }
    if (const RuntimeValue* offsets = findValue(slot, "offsets")) {
        const RuntimeValue::Map& map =
            requireMap(*offsets, source + ".offsets");
        requireOnlyKeys(map, {"left", "top", "right", "bottom"},
                        source + ".offsets");
        if (const RuntimeValue* left = findValue(map, "left")) {
            result.offsetLeft = requireFloat(*left, source + ".offsets.left");
        }
        if (const RuntimeValue* top = findValue(map, "top")) {
            result.offsetTop = requireFloat(*top, source + ".offsets.top");
        }
        if (const RuntimeValue* right = findValue(map, "right")) {
            result.offsetRight =
                requireFloat(*right, source + ".offsets.right");
        }
        if (const RuntimeValue* bottom = findValue(map, "bottom")) {
            result.offsetBottom =
                requireFloat(*bottom, source + ".offsets.bottom");
        }
    }
    if (const RuntimeValue* alignment = findValue(slot, "alignment")) {
        result.alignment = requireVector2f(*alignment, source + ".alignment");
    }
    if (const RuntimeValue* autoSize = findValue(slot, "autoSize")) {
        result.autoSize = requireBool(*autoSize, source + ".autoSize");
    }
    if (const RuntimeValue* zOrder = findValue(slot, "zOrder")) {
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

std::filesystem::path validateLogicalAssetKey(const std::string& assetKey) {
    if (assetKey.empty()) {
        throw std::invalid_argument("UI asset key cannot be empty");
    }
    if (assetKey.find('\\') != std::string::npos) {
        throw std::invalid_argument("UI asset key must use forward slashes: " +
                                    assetKey);
    }
    if (assetKey.find(':') != std::string::npos) {
        throw std::invalid_argument("UI asset key cannot contain a colon: " +
                                    assetKey);
    }
    if (assetKey.front() == '/' || assetKey.back() == '/' ||
        assetKey.find("//") != std::string::npos) {
        throw std::invalid_argument("UI asset key must be canonical: " +
                                    assetKey);
    }
    std::filesystem::path relative =
        ludork::standard::pathFromUtf8(assetKey);
    if (relative.empty() || relative.is_absolute() ||
        relative.has_root_path()) {
        throw std::invalid_argument("UI asset key must be relative: " +
                                    assetKey);
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "." || part == "..") {
            throw std::invalid_argument(
                "UI asset key cannot contain relative path segments: " +
                assetKey);
        }
    }
    if (ludork::standard::pathToGenericUtf8(relative.filename()).find('.') !=
        std::string::npos) {
        throw std::invalid_argument(
            "UI asset key must not include a file extension: " + assetKey);
    }
    const std::string generic =
        ludork::standard::pathToGenericUtf8(relative);
    if (generic != assetKey) {
        throw std::invalid_argument("UI asset key must be canonical: " +
                                    assetKey);
    }
    static const std::vector<std::string> forbiddenPrefixes = {
        "Assets", "UI/Assets", "Data/UI/Assets"};
    for (const std::string& prefix : forbiddenPrefixes) {
        if (generic == prefix || generic.starts_with(prefix + "/")) {
            throw std::invalid_argument(
                "UI asset key must be relative to Data/UI/Assets: " + assetKey);
        }
    }
    return relative;
}

bool isWithinDirectory(const std::filesystem::path& path,
                       const std::filesystem::path& directory) {
    const std::filesystem::path relative = path.lexically_relative(directory);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    const auto first = relative.begin();
    return first != relative.end() && *first != "..";
}

std::optional<std::filesystem::path> findExactChild(
    const std::filesystem::path& directory, const std::string& name,
    bool requireDirectory) {
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(directory)) {
        if (ludork::standard::pathToUtf8(entry.path().filename()) != name) {
            continue;
        }
        if (requireDirectory && !entry.is_directory()) {
            break;
        }
        if (!requireDirectory && !entry.is_regular_file()) {
            break;
        }
        return entry.path();
    }
    return std::nullopt;
}

std::filesystem::path requireExactChild(const std::filesystem::path& directory,
                                        const std::string& name,
                                        const std::string& assetKey,
                                        bool requireDirectory) {
    const std::optional<std::filesystem::path> result =
        findExactChild(directory, name, requireDirectory);
    if (result.has_value()) {
        return *result;
    }
    throw std::out_of_range(
        "UI asset key was not found or does not match filesystem case: " +
        assetKey);
}

std::filesystem::path assetPath(const std::string& assetKey) {
    const std::filesystem::path relative = validateLogicalAssetKey(assetKey);
    const std::filesystem::path root = std::filesystem::weakly_canonical(
        std::filesystem::current_path() / "Data" / "UI" / "Assets");
    if (!std::filesystem::is_directory(root)) {
        throw std::invalid_argument(
            "UI asset directory was not found: Data/UI/Assets");
    }
    std::filesystem::path current = root;
    std::vector<std::string> parts;
    for (const std::filesystem::path& part : relative) {
        parts.push_back(ludork::standard::pathToGenericUtf8(part));
    }
    for (std::size_t index = 0; index + 1 < parts.size(); ++index) {
        current = requireExactChild(current, parts[index], assetKey, true);
    }
    const std::string jsonName = parts.back() + ".json";
    const std::string encryptedName = parts.back() + ".ldc";
    std::optional<std::filesystem::path> dataPath =
        findExactChild(current, jsonName, false);
    if (!dataPath.has_value()) {
        dataPath = findExactChild(current, encryptedName, false);
    }
    if (!dataPath.has_value()) {
        throw std::out_of_range(
            "UI asset key was not found or does not match filesystem case: " +
            assetKey);
    }
    const std::filesystem::path resolved =
        std::filesystem::weakly_canonical(*dataPath);
    if (!isWithinDirectory(resolved, root)) {
        throw std::invalid_argument("UI asset path escapes Data/UI/Assets: " +
                                    assetKey);
    }
    std::filesystem::path logicalPath = *dataPath;
    logicalPath.replace_extension(".json");
    return logicalPath;
}

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

std::shared_ptr<UiAssetInstance> buildAsset(
    const RuntimeValue& value, const std::string& expectedAssetKey,
    BuildContext& context,
    std::optional<sf::Vector2f> logicalSize = std::nullopt);

std::shared_ptr<UiRuntimeNode> buildNode(
    const RuntimeValue& value, const std::string& source,
    UiAssetInstanceState& state, BuildContext& context,
    std::unordered_set<std::string>& nodeIds,
    std::unordered_set<std::string>& localNames, bool root);

void attachChildren(const std::shared_ptr<UiRuntimeNode>& node,
                    const std::string& source) {
    if (node->nestedAsset != nullptr) {
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
                           const RuntimeValue::Map& properties,
                           const std::string& source) {
    if (const RuntimeValue* visible = findValue(properties, "visible")) {
        node.control->setVisible(requireBool(*visible, source + ".visible"));
    }
    if (const RuntimeValue* rotation = findValue(properties, "rotation")) {
        node.control->setRotationDegrees(
            requireFloat(*rotation, source + ".rotation"));
    }
    if (const RuntimeValue* scale = findValue(properties, "scale")) {
        node.renderScale = requireVector2f(*scale, source + ".scale");
        if (node.renderScale.x < 0.0f || node.renderScale.y < 0.0f) {
            throw std::invalid_argument(source + ".scale cannot be negative");
        }
        node.control->setScale(node.renderScale);
    }
    if (const RuntimeValue* origin = findValue(properties, "origin")) {
        node.control->setOrigin(requireVector2f(*origin, source + ".origin"));
    }
}

RuntimeValue::Map effectiveProperties(const RuntimeValue::Map& node,
                                      const RuntimeValue::Map& properties,
                                      const std::string& controlId,
                                      bool designMode,
                                      const std::string& source) {
    RuntimeValue::Map result = properties;
    if (!designMode) {
        return result;
    }
    const RuntimeValue* editorValue = findValue(node, "editor");
    if (editorValue == nullptr) {
        return result;
    }
    const RuntimeValue::Map& editor =
        requireMap(*editorValue, source + ".editor");
    const RuntimeValue* previewText = findValue(editor, "previewText");
    if (previewText == nullptr) {
        return result;
    }
    if (controlId == "Engine.DropBox") {
        result.insert_or_assign(
            "previewText", RuntimeValue(requireString(
                               *previewText, source + ".editor.previewText")));
        return result;
    }
    if (controlId != "Engine.PlainText" &&
        controlId != "Engine.RichText" &&
        controlId != "Engine.FunctionalPlainText" &&
        controlId != "Engine.FunctionalRichText") {
        throw std::invalid_argument(
            source + ".editor.previewText is only valid on text controls");
    }
    result.insert_or_assign(
        "text", RuntimeValue(requireString(*previewText,
                                           source + ".editor.previewText")));
    return result;
}

std::shared_ptr<UiRuntimeNode> buildNode(
    const RuntimeValue& value, const std::string& source,
    UiAssetInstanceState& state, BuildContext& context,
    std::unordered_set<std::string>& nodeIds,
    std::unordered_set<std::string>& localNames, bool root) {
    const RuntimeValue::Map& data = requireMap(value, source);
    const RuntimeValue* idValue = findValue(data, "id");
    const RuntimeValue* nameValue = findValue(data, "name");
    const RuntimeValue* controlIdValue = findValue(data, "controlId");
    const RuntimeValue* propertiesValue = findValue(data, "properties");
    const RuntimeValue* childrenValue = findValue(data, "children");
    if (idValue == nullptr || nameValue == nullptr ||
        controlIdValue == nullptr || propertiesValue == nullptr ||
        childrenValue == nullptr) {
        throw std::invalid_argument(
            source + " requires id, name, controlId, properties, and children");
    }

    std::shared_ptr<UiRuntimeNode> result = std::make_shared<UiRuntimeNode>();
    result->id = requireString(*idValue, source + ".id");
    result->name = requireString(*nameValue, source + ".name");
    result->controlId = requireString(*controlIdValue, source + ".controlId");
    if (result->id.empty() || result->name.empty() ||
        result->controlId.empty()) {
        throw std::invalid_argument(source +
                                    " id, name, and controlId cannot be empty");
    }
    if (!nodeIds.insert(result->id).second) {
        throw std::invalid_argument("Duplicate UI node id " + result->id +
                                    " in " + state.assetKey);
    }
    if (!localNames.insert(result->name).second) {
        throw std::invalid_argument("Duplicate UI node name " + result->name +
                                    " in " + state.assetKey);
    }

    const RuntimeValue::Map& storedProperties =
        requireMap(*propertiesValue, source + ".properties");
    const RuntimeValue::Array& children =
        requireArray(*childrenValue, source + ".children");

    const RuntimeValue* slotValue = findValue(data, "slot");
    if (root) {
        if (slotValue != nullptr) {
            throw std::invalid_argument(source +
                                        " root node cannot have a slot");
        }
    } else if (slotValue == nullptr) {
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
        result->nestedAsset = buildAsset(childAsset, childAssetKey, context);
        result->control = result->nestedAsset->getRoot();
        state.nestedAssets.emplace(result->name, result->nestedAsset);
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
        applyCommonProperties(*result, properties, source + ".properties");
        state.controls.emplace(result->name, result);
    }
    result->control->setName(result->name);

    result->children.reserve(children.size());
    for (std::size_t index = 0; index < children.size(); ++index) {
        const std::string childSource =
            source + ".children[" + std::to_string(index) + "]";
        std::shared_ptr<UiRuntimeNode> child =
            buildNode(children[index], childSource, state, context, nodeIds,
                      localNames, false);
        const RuntimeValue::Map& childData =
            requireMap(children[index], childSource);
        const RuntimeValue* childSlot = findValue(childData, "slot");
        if (childSlot == nullptr) {
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
                const RuntimeValue::Map& listSlot =
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

std::shared_ptr<UiAssetInstance> buildAsset(
    const RuntimeValue& value, const std::string& expectedAssetKey,
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
        const RuntimeValue::Map& asset =
            requireMap(value, "UI asset " + expectedAssetKey);
        requireOnlyKeys(asset, {"type", "designSize", "palette", "root"},
                        "UI asset " + expectedAssetKey);
        const RuntimeValue* type = findValue(asset, "type");
        const RuntimeValue* paletteValue = findValue(asset, "palette");
        const RuntimeValue* rootValue = findValue(asset, "root");
        if (type == nullptr ||
            requireString(*type, expectedAssetKey + ".type") != "uiAsset") {
            throw std::invalid_argument(expectedAssetKey +
                                        " must be a uiAsset");
        }
        if (paletteValue == nullptr) {
            throw std::invalid_argument(expectedAssetKey +
                                        " is missing palette");
        }
        const RuntimeValue::Map& palette =
            requireMap(*paletteValue, expectedAssetKey + ".palette");
        if (nested) {
            const RuntimeValue* exposed = findValue(palette, "exposed");
            if (exposed == nullptr ||
                !requireBool(*exposed, expectedAssetKey + ".palette.exposed")) {
                throw std::invalid_argument(
                    "Nested UI asset must be exposed: " +
                    assetReferenceChain(context));
            }
        }
        if (rootValue == nullptr) {
            throw std::invalid_argument(expectedAssetKey + " is missing root");
        }

        std::shared_ptr<UiAssetInstanceState> state =
            std::make_shared<UiAssetInstanceState>();
        state->assetKey = expectedAssetKey;
        state->designSize =
            parseDesignSize(asset, "UI asset " + expectedAssetKey);
        state->logicalSize = logicalSize.value_or(state->designSize);
        std::unordered_set<std::string> nodeIds;
        std::unordered_set<std::string> localNames;
        state->root = buildNode(*rootValue, expectedAssetKey + ".root", *state,
                                context, nodeIds, localNames, true);

        std::shared_ptr<UiAssetInstance> result(new UiAssetInstance(state));
        UiLayoutEngine::reflow(*result, state->logicalSize);
        context.assetStack.pop_back();
        return result;
    } catch (...) {
        context.assetStack.pop_back();
        throw;
    }
}

void collectNodeViews(const std::shared_ptr<UiRuntimeNode>& node,
                      std::vector<UiAssetNodeView>& result,
                      std::size_t& drawOrder) {
    UiAssetNodeView view;
    view.nodeId = node->id;
    view.control = node->control;
    view.bounds = node->control->getAbsoluteBounds();
    view.nestedBoundary = node->nestedAsset != nullptr;
    view.zOrder = node->canvasSlot.zOrder;
    view.drawOrder = drawOrder++;
    result.push_back(std::move(view));
    if (node->nestedAsset != nullptr) {
        return;
    }
    std::vector<std::shared_ptr<UiRuntimeNode>> children = node->children;
    if (UiControlAdapterRegistry::instance().slotType(node->controlId) ==
        UiControlSlotType::Canvas) {
        std::stable_sort(children.begin(), children.end(),
                         [](const std::shared_ptr<UiRuntimeNode>& left,
                            const std::shared_ptr<UiRuntimeNode>& right) {
                             return left->canvasSlot.zOrder <
                                    right->canvasSlot.zOrder;
                         });
    }
    for (const std::shared_ptr<UiRuntimeNode>& child : children) {
        collectNodeViews(child, result, drawOrder);
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
    return buildAsset(loader(assetKey), assetKey, context, size);
}

}  // namespace

UiAssetInstance::UiAssetInstance(std::shared_ptr<UiAssetInstanceState> state)
    : state_(std::move(state)) {
    if (state_ == nullptr || state_->root == nullptr) {
        throw std::invalid_argument(
            "UI asset instance state must not be empty");
    }
}

UiAssetInstance::~UiAssetInstance() = default;

std::shared_ptr<ControlBase> UiAssetInstance::getRoot() const {
    return state_->root->control;
}

std::shared_ptr<ControlBase> UiAssetInstance::requireControl(
    const std::string& localName) const {
    const auto iterator = state_->controls.find(localName);
    if (iterator != state_->controls.end()) {
        return iterator->second->control;
    }
    if (state_->nestedAssets.contains(localName)) {
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
    const auto iterator = state_->nestedAssets.find(localName);
    if (iterator == state_->nestedAssets.end()) {
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
        if (state_->nestedAssets.contains(localName)) {
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
        if (state_->nestedAssets.contains(localName)) {
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
    UiLayoutEngine::reflow(*this, state_->logicalSize);
}

std::vector<UiAssetNodeView> UiAssetInstance::getNodeViews() const {
    std::vector<UiAssetNodeView> result;
    std::size_t drawOrder = 0;
    collectNodeViews(state_->root, result, drawOrder);
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
