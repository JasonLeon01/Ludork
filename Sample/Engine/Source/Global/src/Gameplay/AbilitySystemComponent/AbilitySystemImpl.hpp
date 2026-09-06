#pragma once
#include "NumericImpl.hpp"
#include <Gameplay/AttributeSet.hpp>
#include <Gameplay/GameplayAbilitySpec.hpp>
#include <Gameplay/ActiveGameplayEffect.hpp>
#include <functional>

namespace ludork::global::ability_system_impl {

struct AbilitySystemImpl {
    struct ModifierAggregate {
        NumericValue additive{0.0, true};
        NumericValue multiplier{1.0, true};
        std::optional<NumericValue> overrideValue;
        std::optional<NumericValue> minimum;
    };

    struct Listener {
        RuntimeHandle callback;
        RuntimeValue::Array params;
    };

    std::shared_ptr<AttributeSet> attributeSet;
    std::vector<std::string> numericAttributes;
    AttributeNumbers baseValues;
    std::vector<std::shared_ptr<GameplayAbilitySpec>> abilities;
    std::unordered_map<int, std::shared_ptr<ActiveGameplayEffect>>
        activeEffects;
    std::vector<int> activeEffectOrder;
    std::unordered_map<std::string, int> tagCounts;
    std::unordered_map<std::string, std::vector<Listener>> listeners;
    std::unordered_map<std::string, RuntimeHandle> constraints;
    int nextAbilityOrder = 1;
    int nextEffectHandle = 1;
    int nextEffectOrder = 1;
    int revision = 0;
    bool internalAttributeWrite = false;
    bool suppressAttributeListeners = false;

    std::function<RuntimeValue()> selfValue;
};

}  // namespace ludork::global::ability_system_impl
