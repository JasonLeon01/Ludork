#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Gameplay/GameplayNumber.hpp>
#include <Runtime/StrictFunction.hpp>

class GameplayAbilityResult;
class GameplayEventData;
class GameplayAbility;
class GameplayEffectSpec;
class GameplayAbilitySpec;
class ActiveGameplayEffect;
class AttributeSet;

BIND_CLASS()
class LUDORK_GLOBAL_API AbilitySystemComponent : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(AbilitySystemComponent, RuntimeObject)

    struct Impl;
    using NumericConstraint = ludork::runtime::StrictFunction<GameplayNumber(
        GameplayNumber, std::shared_ptr<AbilitySystemComponent>,
        const GameplayNumbers&)>;

    BIND_INIT(parameter_types = {any, AttributeSet})
    AbilitySystemComponent(RuntimeValue owner,
                           std::shared_ptr<AttributeSet> attributeSet);
    ~AbilitySystemComponent() override;

    AbilitySystemComponent(const AbilitySystemComponent&) = delete;
    AbilitySystemComponent& operator=(const AbilitySystemComponent&) = delete;

    BIND_METHOD(Pure = true)
    RuntimeValue getOwner() const;

    BIND_METHOD(Pure = true)
    std::shared_ptr<AttributeSet> getAttributeSet() const;

    BIND_METHOD(Pure = true)
    GameplayNumber getNumericAttribute(const std::string& name) const;

    BIND_METHOD(Pure = true)
    GameplayNumber getNumericAttributeBase(const std::string& name) const;

    BIND_METHOD()
    void setNumericAttributeBase(const std::string& name,
                                 const GameplayNumber& value);

    BIND_METHOD()
    void setNumericAttributeBases(const GameplayNumbers& values);

    BIND_METHOD(Pure = true)
    GameplayNumbers getNumericAttributeBases() const;

    BIND_METHOD(defaults = {nil, nil, {}},
                parameter_types = {string, function, any[]})
    void addAttributeChangeListener(const std::string& name,
                                    RuntimeIdentityPtr callback,
                                    RuntimeValue::Array params = {});

    BIND_METHOD(defaults = {nil})
    void setNumericAttributeConstraint(const std::string& name,
                                       NumericConstraint callback = {});

    BIND_METHOD(defaults = {nil}, parameter_types = {GameplayAbility, any})
    std::shared_ptr<GameplayAbilitySpec> giveAbility(
        std::shared_ptr<GameplayAbility> ability, RuntimeValue sourceKey = {});

    BIND_METHOD(parameter_types = {any})
    void removeAbilitiesBySource(const RuntimeValue& sourceKey);

    BIND_METHOD(defaults = {nil})
    std::shared_ptr<GameplayAbilityResult> tryActivateAbility(
        const std::string& abilityID,
        std::shared_ptr<GameplayEventData> eventData = {});

    BIND_METHOD()
    std::vector<std::shared_ptr<GameplayAbilityResult>> handleGameplayEvent(
        const std::shared_ptr<GameplayEventData>& eventData);

    BIND_METHOD()
    std::optional<int> applyGameplayEffectSpec(
        const std::shared_ptr<GameplayEffectSpec>& spec);

    BIND_METHOD(Pure = true)
    bool validateGameplayEffectSpec(
        const std::shared_ptr<GameplayEffectSpec>& spec) const;

    BIND_METHOD(defaults = {nil})
    bool removeActiveGameplayEffect(int handle,
                                    std::optional<int> stacks = std::nullopt);

    BIND_METHOD(Pure = true)
    int getActiveEffectStacks(const std::string& effectID) const;

    BIND_METHOD(Pure = true)
    std::vector<std::shared_ptr<ActiveGameplayEffect>>
    getActiveGameplayEffects() const;

    BIND_METHOD(Pure = true)
    bool hasMatchingGameplayTag(const std::string& tag) const;

    BIND_METHOD(Pure = true)
    int getRevision() const;

    BIND_METHOD(metadata = false)
    void onAttributeWrite(const std::string& name, const RuntimeValue& oldValue,
                          const RuntimeValue& newValue);

private:
    std::unique_ptr<Impl> impl_;
};
