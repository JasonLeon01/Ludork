#include "UiControlAdapters/ChildPolicyName.hpp"
#include <UI/UiControlAdapterRegistry.hpp>
#include <Sha256.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string_view childPolicyName(UiChildPolicy policy) {
    const auto name =
        ludork::engine::ui_control_adapter_detail::childPolicyName(policy);
    if (!name.has_value()) {
        throw std::logic_error("Unknown UI child policy");
    }
    return *name;
}

std::string_view slotTypeName(UiControlSlotType slotType) {
    switch (slotType) {
        case UiControlSlotType::None:
            return {};
        case UiControlSlotType::Canvas:
            return "canvas";
        case UiControlSlotType::List:
            return "list";
    }
    throw std::logic_error("Unknown UI Slot type");
}

std::string adapterFingerprintSource() {
    std::vector<const UiControlAdapterDescriptor*> descriptors;
    descriptors.reserve(uiControlAdapterDescriptorTable.size());
    for (const UiControlAdapterDescriptor& descriptor :
         uiControlAdapterDescriptorTable) {
        descriptors.push_back(&descriptor);
    }
    std::sort(descriptors.begin(), descriptors.end(),
              [](const UiControlAdapterDescriptor* left,
                 const UiControlAdapterDescriptor* right) {
                  return left->controlId < right->controlId;
              });

    std::string source;
    for (const UiControlAdapterDescriptor* descriptor : descriptors) {
        source.append(descriptor->controlId);
        source.push_back('|');
        source.append(descriptor->adapter);
        source.push_back('|');
        source.append(childPolicyName(descriptor->childPolicy));
        source.push_back('|');
        source.append(slotTypeName(descriptor->slotType));
        source.push_back('|');
        for (const UiControlPropertyDescriptor& property :
             descriptor->properties) {
            if (property.editorOnly) {
                continue;
            }
            source.append(property.id);
            source.push_back(':');
            source.append(property.type);
            source.push_back(':');
            source.append(property.required ? "true" : "false");
            source.push_back(';');
        }
        source.push_back('\n');
    }
    return source;
}

}  // namespace

std::string_view uiControlAdapterFingerprint() {
    static const std::string fingerprint =
        ludork::standard::sha256(adapterFingerprintSource());
    return fingerprint;
}

std::string_view uiControlRegistryHash() {
    static const std::string hash =
        ludork::standard::sha256(uiControlRegistryDescription());
    return hash;
}
