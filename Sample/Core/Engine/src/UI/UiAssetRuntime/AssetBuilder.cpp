#include "AssetBuilder.hpp"

#include <Runtime/RuntimeValueReader.hpp>

#include <stdexcept>

namespace ludork::engine::ui_asset_runtime_impl {
namespace {

constexpr const char* ProjectControlPrefix = "Project:";

using ludork::runtime::value_reader::findValue;
using ludork::runtime::value_reader::requireFloat;
using ludork::runtime::value_reader::requireMap;
using ludork::runtime::value_reader::requireString;

}  // namespace

bool isProjectControl(const std::string& controlId) {
    return controlId.starts_with(ProjectControlPrefix);
}

std::string nestedAssetKey(const std::string& controlId) {
    const std::size_t prefixLength =
        std::char_traits<char>::length(ProjectControlPrefix);
    if (!isProjectControl(controlId) || controlId.size() == prefixLength) {
        throw std::invalid_argument("Invalid project UI control id: " +
                                    controlId);
    }
    return controlId.substr(prefixLength);
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
    if (controlId != "Engine.PlainText" && controlId != "Engine.RichText" &&
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

}  // namespace ludork::engine::ui_asset_runtime_impl
