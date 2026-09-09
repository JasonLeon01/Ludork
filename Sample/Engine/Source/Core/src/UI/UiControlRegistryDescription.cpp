#include <UI/UiControlAdapterRegistry.hpp>

#include "UiControlAdapters/UiControlPropertyCodec.hpp"

#include <Runtime/Json.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

bool identifier(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    const auto letter = [](char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') || character == '_';
    };
    return letter(value.front()) &&
           std::all_of(value.begin(), value.end(), [&](char character) {
               return letter(character) ||
                      (character >= '0' && character <= '9');
           });
}

bool controlIdentifier(std::string_view value) {
    bool qualified = false;
    while (true) {
        const std::size_t separator = value.find('.');
        if (!identifier(value.substr(0, separator))) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return qualified;
        }
        qualified = true;
        value.remove_prefix(separator + 1);
    }
}

std::string_view childPolicyName(UiChildPolicy policy) {
    switch (policy) {
        case UiChildPolicy::None:
            return "none";
        case UiChildPolicy::Single:
            return "single";
        case UiChildPolicy::Multiple:
            return "multiple";
    }
    throw std::invalid_argument("Unknown UI child policy");
}

RuntimeData slotTypeValue(UiControlSlotType type) {
    switch (type) {
        case UiControlSlotType::None:
            return RuntimeData();
        case UiControlSlotType::Canvas:
            return RuntimeData("canvas");
        case UiControlSlotType::List:
            return RuntimeData("list");
    }
    throw std::invalid_argument("Unknown UI Slot type");
}

RuntimeData textKindValue(const UiControlAdapterDescriptor& descriptor) {
    UiControlPropertyDescriptor::TextKind kind =
        UiControlPropertyDescriptor::TextKind::None;
    for (const UiControlPropertyDescriptor& property : descriptor.properties) {
        if (property.textKind == UiControlPropertyDescriptor::TextKind::None) {
            continue;
        }
        if ((property.textKind !=
                 UiControlPropertyDescriptor::TextKind::Plain &&
             property.textKind !=
                 UiControlPropertyDescriptor::TextKind::Rich) ||
            (kind != UiControlPropertyDescriptor::TextKind::None &&
             kind != property.textKind) ||
            !property.adapterProperty || property.editorOnly) {
            throw std::invalid_argument("Invalid UI text property group: " +
                                        std::string(descriptor.controlId));
        }
        kind = property.textKind;
    }
    if (kind == UiControlPropertyDescriptor::TextKind::Plain) {
        return RuntimeData("plain");
    }
    if (kind == UiControlPropertyDescriptor::TextKind::Rich) {
        return RuntimeData("rich");
    }
    return RuntimeData();
}

void appendField(std::string& output, std::string_view name,
                 const RuntimeData& value) {
    if (output.back() != '{') {
        output.push_back(',');
    }
    output += stringifyJSON(RuntimeData(std::string(name)));
    output.push_back(':');
    output += stringifyJSON(value);
}

void appendStringField(std::string& output, std::string_view name,
                       std::string_view value) {
    appendField(output, name, RuntimeData(std::string(value)));
}

std::string describeRegistry() {
    static_cast<void>(UiControlAdapterRegistry::instance());
    std::string output = "{";
    appendField(output, "formatVersion", RuntimeData(std::int64_t{1}));
    appendStringField(output, "adapterFingerprint",
                      uiControlAdapterFingerprint());
    output += ",\"controls\":[";
    bool firstControl = true;
    for (const UiControlAdapterDescriptor& descriptor :
         uiControlAdapterDescriptors()) {
        if (!firstControl) {
            output.push_back(',');
        }
        firstControl = false;
        output.push_back('{');
        appendStringField(output, "controlId", descriptor.controlId);
        appendStringField(output, "source", "system");
        appendStringField(output, "displayName", descriptor.displayName);
        appendStringField(output, "category", descriptor.category);
        appendStringField(output, "adapter", descriptor.adapter);
        appendStringField(output, "childPolicy",
                          childPolicyName(descriptor.childPolicy));
        appendField(output, "slotType", slotTypeValue(descriptor.slotType));
        appendField(output, "textKind", textKindValue(descriptor));
        output += ",\"properties\":[";
        bool firstProperty = true;
        for (const UiControlPropertyDescriptor& property :
             descriptor.properties) {
            if (!firstProperty) {
                output.push_back(',');
            }
            firstProperty = false;
            output.push_back('{');
            appendStringField(output, "id", property.id);
            appendStringField(output, "displayName", property.displayName);
            appendStringField(output, "type", property.type);
            appendField(output, "required", RuntimeData(property.required));
            appendField(output, "default",
                        ui_control_adapter_detail::parsePropertyDefault(
                            property, std::string(descriptor.controlId) + "." +
                                          std::string(property.id)));
            appendField(output, "editorOnly", RuntimeData(property.editorOnly));
            appendField(output, "adapterProperty",
                        RuntimeData(property.adapterProperty));
            output.push_back('}');
        }
        output += "]}";
    }
    output += "]}\n";
    return output;
}

}  // namespace

void validateUiControlAdapterDescriptors() {
    std::unordered_set<std::string_view> controls;
    for (const UiControlAdapterDescriptor& descriptor :
         uiControlAdapterDescriptors()) {
        const std::string source(descriptor.controlId);
        if (!controlIdentifier(descriptor.controlId) ||
            !controls.emplace(descriptor.controlId).second ||
            descriptor.adapter != descriptor.controlId ||
            descriptor.displayName.empty() || descriptor.category.empty()) {
            throw std::invalid_argument("Invalid or duplicate UI descriptor: " +
                                        source);
        }
        static_cast<void>(childPolicyName(descriptor.childPolicy));
        static_cast<void>(slotTypeValue(descriptor.slotType));
        static_cast<void>(textKindValue(descriptor));
        if (descriptor.properties.size() <
            uiControlCommonPropertyDescriptors.size()) {
            throw std::invalid_argument("Missing common UI properties: " +
                                        source);
        }
        std::unordered_set<std::string_view> properties;
        for (std::size_t index = 0; index < descriptor.properties.size();
             ++index) {
            const UiControlPropertyDescriptor& property =
                descriptor.properties[index];
            const std::string propertySource =
                source + "." + std::string(property.id);
            if (!identifier(property.id) || property.displayName.empty() ||
                !properties.emplace(property.id).second ||
                (property.editorOnly && property.adapterProperty)) {
                throw std::invalid_argument(
                    "Invalid or duplicate UI property: " + propertySource);
            }
            if (index < uiControlCommonPropertyDescriptors.size()) {
                const UiControlPropertyDescriptor& common =
                    uiControlCommonPropertyDescriptors[index];
                if (property.id != common.id || property.type != common.type ||
                    property.displayName != common.displayName ||
                    property.defaultJson != common.defaultJson ||
                    property.required != common.required ||
                    property.editorOnly || property.adapterProperty ||
                    property.textKind !=
                        UiControlPropertyDescriptor::TextKind::None) {
                    throw std::invalid_argument("Invalid common UI property: " +
                                                propertySource);
                }
            } else if (!property.editorOnly && !property.adapterProperty) {
                throw std::invalid_argument("UI property has no owner: " +
                                            propertySource);
            }
            static_cast<void>(ui_control_adapter_detail::parsePropertyDefault(
                property, propertySource));
        }
    }
}

std::string_view uiControlRegistryDescription() {
    static const std::string description = describeRegistry();
    return description;
}
