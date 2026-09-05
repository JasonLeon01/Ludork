#include <UI/UiControlAdapterRegistry.hpp>

#include "UiControlAdapters/UiControlAdapterRegistryBuilder.hpp"

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

std::shared_ptr<ControlBase> UiControlAdapterRegistry::create(
    const std::string& controlId, const RuntimeValue::Map& properties) const {
    return requireAdapter(controlId).factory(properties);
}

void UiControlAdapterRegistry::setProperty(const std::string& controlId,
                                           ControlBase& control,
                                           const std::string& propertyId,
                                           const RuntimeValue& value) const {
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
    Builder::registerLayoutAdapters(*this);
    Builder::registerVisualAdapters(*this);
    Builder::registerInputAdapters(*this);
    Builder::registerSkinnedAdapters(*this);
    Builder::registerTextAdapters(*this);
}

const UiControlAdapterRegistry::Adapter&
UiControlAdapterRegistry::requireAdapter(const std::string& controlId) const {
    const auto iterator = adapters_.find(controlId);
    if (iterator == adapters_.end()) {
        throw std::invalid_argument("Unknown UI control adapter: " + controlId);
    }
    return iterator->second;
}
