#pragma once
#include "NumericImpl.hpp"
#include <Gameplay/AttributeSet.hpp>
#include <Gameplay/GameplayAbilitySpec.hpp>
#include <Gameplay/ActiveGameplayEffect.hpp>
#include <functional>

namespace ludork::global::ability_system_impl {

struct AbilitySystemImpl {
    enum class AttributeChangeSource {
        Base,
        Effect,
        Constraint
    };

    struct AttributeChange {
        AttributeChangeSource source;
        bool force = false;
        std::optional<GameplayNumber> oldBase;
        std::optional<GameplayNumber> newBase;
    };

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
    GameplayNumbers baseValues;
    std::vector<std::shared_ptr<GameplayAbilitySpec>> abilities;
    std::unordered_map<int, std::shared_ptr<ActiveGameplayEffect>>
        activeEffects;
    std::vector<int> activeEffectOrder;
    std::unordered_map<std::string, int> tagCounts;
    std::unordered_map<std::string, std::vector<Listener>> listeners;
    int nextAbilityOrder = 1;
    int nextEffectHandle = 1;
    int nextEffectOrder = 1;
    int revision = 0;
    bool internalAttributeWrite = false;
    bool suppressAttributeListeners = false;

    std::function<std::optional<GameplayNumber>(
        const std::string&, GameplayNumber, const GameplayNumbers&)>
        constrain;
};

}  // namespace ludork::global::ability_system_impl
