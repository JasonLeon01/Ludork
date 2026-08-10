#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/ControlBase.hpp>
#include <UI/UiControlAdapterDescriptors.hpp>

#include <SFML/System/Vector2.hpp>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

LUDORK_ENGINE_API std::span<const UiControlAdapterDescriptor>
uiControlAdapterDescriptors();

LUDORK_ENGINE_API std::string_view uiControlAdapterFingerprint();

LUDORK_ENGINE_API void clearUiControlAdapterResourceCache() noexcept;

class LUDORK_ENGINE_API UiControlAdapterRegistry {
public:
    static const UiControlAdapterRegistry& instance();

    bool contains(const std::string& controlId) const;
    UiChildPolicy childPolicy(const std::string& controlId) const;
    UiControlSlotType slotType(const std::string& controlId) const;
    bool supportsProperty(const std::string& controlId,
                          const std::string& propertyId) const;
    std::shared_ptr<ControlBase> create(
        const std::string& controlId,
        const RuntimeValue::Map& properties) const;
    void setProperty(const std::string& controlId, ControlBase& control,
                     const std::string& propertyId,
                     const RuntimeValue& value) const;
    sf::Vector2f measure(const ControlBase& control) const;
    void arrange(const std::string& controlId, ControlBase& control,
                 const sf::Vector2f& size,
                 const sf::Vector2f& renderScale) const;
    void attachChildren(
        const std::string& controlId, ControlBase& control,
        const std::vector<std::shared_ptr<ControlBase>>& children) const;
    void reflowChildren(const std::string& controlId,
                        ControlBase& control) const;

private:
    struct Adapter {
        UiChildPolicy childPolicy = UiChildPolicy::None;
        UiControlSlotType slotType = UiControlSlotType::None;
        std::unordered_set<std::string> properties;
        std::function<std::shared_ptr<ControlBase>(
            const RuntimeValue::Map& properties)>
            factory;
        std::function<void(ControlBase& control, const std::string& propertyId,
                           const RuntimeValue& value)>
            setter;
        std::function<void(ControlBase& control, const sf::Vector2f& size,
                           const sf::Vector2f& renderScale)>
            arranger;
        std::function<void(
            ControlBase& control,
            const std::vector<std::shared_ptr<ControlBase>>& children)>
            childAttacher;
        std::function<void(ControlBase& control)> childReflow;
    };

    UiControlAdapterRegistry();

    template <typename Tag>
    void registerAdapter(Adapter adapter) {
        const UiControlAdapterDescriptor& descriptor =
            UiControlAdapterTraits<Tag>::descriptor;
        adapter.childPolicy = descriptor.childPolicy;
        adapter.slotType = descriptor.slotType;
        for (const UiControlPropertyDescriptor& property :
             descriptor.properties) {
            if (property.adapterProperty) {
                adapter.properties.emplace(property.id);
            }
        }
        adapters_.emplace(std::string(descriptor.controlId),
                          std::move(adapter));
    }

    const Adapter& requireAdapter(const std::string& controlId) const;

    std::unordered_map<std::string, Adapter> adapters_;
};
