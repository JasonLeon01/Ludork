#pragma once

#include <CoreMinimal.hpp>

#include <GlobalRuntimeApi.hpp>

class AbilitySystemComponent;
class GameplayAbility;
class GameplayAbilitySpec;
class GameplayEffect;
class GameplayEffectSpec;
class GameplayEventData;

BIND_CLASS(copyable = true, table_init = true)
struct GameplayModifier {
    BIND_PROPERTY()
    std::string attribute;

    BIND_PROPERTY()
    std::string operation;

    BIND_PROPERTY(type = any)
    RuntimeValue magnitude;

    BIND_PROPERTY()
    RuntimeValue minimum;
};

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayAbilityResult : public RuntimeObject {
public:
    BIND_INIT(defaults = {nil, nil}, parameter_types = {bool, any, any})
    explicit GameplayAbilityResult(bool succeeded, RuntimeValue resultCode = {},
                                   RuntimeIdentityPtr resultData = {});

    BIND_PROPERTY()
    bool ok = false;

    BIND_PROPERTY(type = any)
    RuntimeValue code;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr data;

    BIND_METHOD(Pure = true, defaults = {nil, nil},
                parameter_types = {any, any})
    static std::shared_ptr<GameplayAbilityResult> Success(
        RuntimeValue code = {}, RuntimeIdentityPtr data = {});

    BIND_METHOD(Pure = true, defaults = {nil}, parameter_types = {any, any})
    static std::shared_ptr<GameplayAbilityResult> Failure(
        RuntimeValue code, RuntimeIdentityPtr data = {});
};

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayEventData : public RuntimeObject {
public:
    BIND_INIT(defaults = {nil, nil, "", nil},
              parameter_types = {any, any, string, any})
    explicit GameplayEventData(RuntimeValue eventInstigator = {},
                               RuntimeValue eventTarget = {},
                               std::string tag = "",
                               RuntimeIdentityPtr eventPayload = {});

    BIND_PROPERTY(type = any)
    RuntimeValue instigator;

    BIND_PROPERTY(type = any)
    RuntimeValue target;

    BIND_PROPERTY()
    std::string eventTag;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr payload;
};

BIND_CLASS(callbacks = true, table_init = true)
class LUDORK_GLOBAL_API GameplayAbility : public RuntimeObject {
public:
    virtual ~GameplayAbility() = default;

    BIND_PROPERTY()
    std::string id;

    BIND_PROPERTY()
    double priority = 0.0;

    BIND_PROPERTY()
    std::vector<std::string> abilityTags;

    BIND_PROPERTY()
    std::vector<std::string> requiredTags;

    BIND_PROPERTY()
    std::vector<std::string> blockedTags;

    BIND_PROPERTY()
    std::vector<std::string> triggerTags;

    BIND_METHOD(Pure = true,
                parameter_types = {AbilitySystemComponent, GameplayEventData})
    virtual std::shared_ptr<GameplayAbilityResult> canActivate(
        const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
        const std::shared_ptr<GameplayEventData>& eventData);

    BIND_METHOD(Pure = true,
                parameter_types = {AbilitySystemComponent, GameplayEventData})
    virtual std::shared_ptr<GameplayAbilityResult> calculate(
        const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
        const std::shared_ptr<GameplayEventData>& eventData);

    BIND_METHOD(parameter_types = {AbilitySystemComponent, GameplayEventData})
    virtual std::shared_ptr<GameplayAbilityResult> activate(
        const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
        const std::shared_ptr<GameplayEventData>& eventData);
};

BIND_CLASS(table_init = true)
class LUDORK_GLOBAL_API GameplayEffect : public RuntimeObject {
public:
    GameplayEffect();

    BIND_PROPERTY()
    std::string id;

    BIND_PROPERTY()
    std::string durationPolicy = "Instant";

    BIND_PROPERTY()
    std::string stackingPolicy = "None";

    BIND_PROPERTY()
    std::vector<GameplayModifier> modifiers;

    BIND_PROPERTY()
    std::vector<std::string> grantedTags;

    BIND_PROPERTY()
    std::vector<std::shared_ptr<GameplayAbility>> grantedAbilities;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr data;
};

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayEffectSpec : public RuntimeObject {
public:
    BIND_INIT(defaults = {nil, 1, nil},
              parameter_types = {GameplayEffect, GameplayEventData, int, any})
    GameplayEffectSpec(std::shared_ptr<GameplayEffect> gameplayEffect,
                       std::shared_ptr<GameplayEventData> gameplayEvent = {},
                       int stackCount = 1, RuntimeValue source = {});

    BIND_PROPERTY()
    std::shared_ptr<GameplayEffect> effect;

    BIND_PROPERTY()
    std::shared_ptr<GameplayEventData> eventData;

    BIND_PROPERTY()
    int stacks = 1;

    BIND_PROPERTY(type = any)
    RuntimeValue sourceKey;
};

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayAbilitySpec : public RuntimeObject {
public:
    BIND_INIT(defaults = {nil, 0},
              parameter_types = {GameplayAbility, any, int})
    GameplayAbilitySpec(std::shared_ptr<GameplayAbility> gameplayAbility,
                        RuntimeValue source = {}, int order = 0);

    BIND_PROPERTY()
    std::shared_ptr<GameplayAbility> ability;

    BIND_PROPERTY(type = any)
    RuntimeValue sourceKey;

    BIND_PROPERTY()
    int grantOrder = 0;
};

BIND_CLASS()
class LUDORK_GLOBAL_API ActiveGameplayEffect : public RuntimeObject {
public:
    BIND_INIT()
    ActiveGameplayEffect(int effectHandle,
                         std::shared_ptr<GameplayEffectSpec> effectSpec,
                         int order);

    BIND_PROPERTY()
    int handle = 0;

    BIND_PROPERTY()
    std::shared_ptr<GameplayEffectSpec> spec;

    BIND_PROPERTY()
    int stacks = 1;

    BIND_PROPERTY()
    int applicationOrder = 0;

    BIND_PROPERTY()
    std::vector<std::shared_ptr<GameplayAbilitySpec>> grantedAbilitySpecs;
};

BIND_CLASS(callbacks = true)
class LUDORK_GLOBAL_API AttributeSet : public RuntimeObject {
public:
    BIND_INIT()
    AttributeSet() = default;

    BIND_METHOD(metadata = false)
    void initialize(const RuntimeValue::Map& values);

    BIND_METHOD(Pure = true)
    std::vector<std::string> getAttributeNames() const;

    BIND_METHOD(Pure = true)
    RuntimeValue getAttributeSchema(const std::string& name) const;

    RuntimeValue getAttributeValue(const std::string& name) const;
    void setAttributeValue(const std::string& name, const RuntimeValue& value);
    std::string getAttributeType(const std::string& name) const;

private:
    RuntimeValue selfValue() const;

    std::vector<std::string> attributeNames_;
    RuntimeValue::Map schema_;
};

BIND_CLASS()
class LUDORK_GLOBAL_API AbilitySystemComponent : public RuntimeObject {
public:
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
    RuntimeValue getNumericAttribute(const std::string& name) const;

    BIND_METHOD(Pure = true)
    RuntimeValue getNumericAttributeBase(const std::string& name) const;

    BIND_METHOD()
    void setNumericAttributeBase(const std::string& name,
                                 const RuntimeValue& value);

    BIND_METHOD()
    void setNumericAttributeBases(const RuntimeValue::Map& values);

    BIND_METHOD(Pure = true)
    RuntimeValue::Map getNumericAttributeBases() const;

    BIND_METHOD(defaults = {nil, nil, {}},
                parameter_types = {string, function, any[]})
    void addAttributeChangeListener(const std::string& name,
                                    RuntimeIdentityPtr callback,
                                    RuntimeValue::Array params = {});

    BIND_METHOD(defaults = {nil}, parameter_types = {string, function})
    void setNumericAttributeConstraint(const std::string& name,
                                       RuntimeIdentityPtr callback = {});

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
    struct State;
    std::unique_ptr<State> state_;
};
