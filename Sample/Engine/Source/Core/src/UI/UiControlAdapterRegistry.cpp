#include <UI/UiControlAdapterRegistry.hpp>

#include "UiControlAdapters/UiControlAdapterRegistryBuilderImpl.hpp"
#include "UiControlAdapters/UiControlPropertyCodec.hpp"

#include <stdexcept>

std::span<const UiControlAdapterDescriptor> uiControlAdapterDescriptors() {
    return uiControlAdapterDescriptorTable;
}

const UiControlAdapterRegistry& UiControlAdapterRegistry::instance() {
    static UiControlAdapterRegistry registry;
    return registry;
}

bool UiControlAdapterRegistry::contains(const std::string& controlId) const {
    return adapters_.contains(controlId);
}

UiChildPolicy UiControlAdapterRegistry::childPolicy(
    const std::string& controlId) const {
    return requireAdapter(controlId).childPolicy;
}

UiControlSlotType UiControlAdapterRegistry::slotType(
    const std::string& controlId) const {
    return requireAdapter(controlId).slotType;
}

bool UiControlAdapterRegistry::supportsProperty(
    const std::string& controlId, const std::string& propertyId) const {
    return requireAdapter(controlId).properties.contains(propertyId);
}

UiControlProperties UiControlAdapterRegistry::parseProperties(
    const std::string& controlId, const RuntimeData::Map& properties,
    const std::string& source) const {
    for (const UiControlAdapterDescriptor& descriptor :
         uiControlAdapterDescriptors()) {
        if (descriptor.controlId == controlId) {
            return ui_control_adapter_detail::parseProperties(
                descriptor, properties, source);
        }
    }
    throw std::invalid_argument("Unknown UI control adapter: " + controlId);
}

std::shared_ptr<ControlBase> UiControlAdapterRegistry::create(
    const std::string& controlId, const UiControlProperties& properties) const {
    return requireAdapter(controlId).factory(properties);
}

void UiControlAdapterRegistry::setProperty(
    const std::string& controlId, ControlBase& control,
    const std::string& propertyId, const UiControlPropertyValue& value) const {
    const Adapter& adapter = requireAdapter(controlId);
    if (!adapter.properties.contains(propertyId)) {
        throw std::invalid_argument("Unknown property " + propertyId +
                                    " for UI control " + controlId);
    }
    adapter.setter(control, propertyId, value);
}

sf::Vector2f UiControlAdapterRegistry::measure(
    const ControlBase& control) const {
    return control.getSize();
}

void UiControlAdapterRegistry::arrange(const std::string& controlId,
                                       ControlBase& control,
                                       const sf::Vector2f& size,
                                       const sf::Vector2f& renderScale) const {
    requireAdapter(controlId).arranger(control, size, renderScale);
}

void UiControlAdapterRegistry::attachChildren(
    const std::string& controlId, ControlBase& control,
    const std::vector<std::shared_ptr<ControlBase>>& children) const {
    const Adapter& adapter = requireAdapter(controlId);
    if (!adapter.childAttacher) {
        throw std::logic_error(
            "UI adapter does not implement child attachment: " + controlId);
    }
    adapter.childAttacher(control, children);
}

void UiControlAdapterRegistry::reflowChildren(const std::string& controlId,
                                              ControlBase& control) const {
    const Adapter& adapter = requireAdapter(controlId);
    if (adapter.childReflow) {
        adapter.childReflow(control);
    }
}

UiControlAdapterRegistry::UiControlAdapterRegistry() {
    validateUiControlAdapterDescriptors();
    BuilderImpl::registerLayoutAdapters(*this);
    BuilderImpl::registerVisualAdapters(*this);
    BuilderImpl::registerInputAdapters(*this);
    BuilderImpl::registerSkinnedAdapters(*this);
    BuilderImpl::registerTextAdapters(*this);
    for (const UiControlAdapterDescriptor& descriptor :
         uiControlAdapterDescriptors()) {
        static_cast<void>(requireAdapter(std::string(descriptor.controlId)));
    }
    if (adapters_.size() != uiControlAdapterDescriptors().size()) {
        throw std::logic_error(
            "Registered UI adapters differ from the descriptor table");
    }
}

void UiControlAdapterRegistry::registerAdapter(const std::string& controlId,
                                               Adapter adapter) {
    if (!adapter.factory || !adapter.setter || !adapter.arranger ||
        (adapter.childPolicy != UiChildPolicy::None &&
         !adapter.childAttacher)) {
        throw std::logic_error("UI adapter is missing required operations: " +
                               controlId);
    }
    if (!adapters_.emplace(controlId, std::move(adapter)).second) {
        throw std::logic_error("Duplicate UI adapter factory: " + controlId);
    }
}

const UiControlAdapterRegistry::Adapter&
UiControlAdapterRegistry::requireAdapter(const std::string& controlId) const {
    const auto iterator = adapters_.find(controlId);
    if (iterator == adapters_.end()) {
        throw std::invalid_argument("Unknown UI control adapter: " + controlId);
    }
    return iterator->second;
}
