#include <Gameplay/GameplayAbilitySystem.hpp>

#include <Runtime/RuntimeValueServices.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

RuntimeIdentityPtr runtimeMap(RuntimeValue::Map values = {}) {
    RuntimeIdentityPtr result = createRuntimeMapIdentity();
    for (auto& [name, value] : values) {
        ludork::engine::runtime_services::invokeVoid(
            "reflect.set",
            {RuntimeValue(result), RuntimeValue(name), std::move(value)});
    }
    return result;
}

bool resultCodeTruthy(const RuntimeValue& code) {
    if (const std::string* value = code.getIf<std::string>()) {
        return !value->empty();
    }
    if (const std::int64_t* value = code.getIf<std::int64_t>()) {
        return *value != 0;
    }
    if (const double* value = code.getIf<double>()) {
        return std::isfinite(*value) && *value != 0.0;
    }
    return false;
}

RuntimeValue cloneRuntimeValue(const RuntimeValue& value) {
    return ludork::engine::runtime_services::invokeFirst("reflect.clone",
                                                         {value});
}

}  // namespace

GameplayAbilityResult::GameplayAbilityResult(bool succeeded,
                                             RuntimeValue resultCode,
                                             RuntimeIdentityPtr resultData)
    : ok(succeeded), code(std::move(resultCode)), data(std::move(resultData)) {
    if (code.isNil()) {
        code = RuntimeValue("");
    }
    if (data == nullptr) {
        data = runtimeMap();
    }
}

std::shared_ptr<GameplayAbilityResult> GameplayAbilityResult::Success(
    RuntimeValue code, RuntimeIdentityPtr data) {
    if (code.isNil()) {
        code = RuntimeValue("Success");
    }
    return std::make_shared<GameplayAbilityResult>(true, std::move(code),
                                                   std::move(data));
}

std::shared_ptr<GameplayAbilityResult> GameplayAbilityResult::Failure(
    RuntimeValue code, RuntimeIdentityPtr data) {
    if (!resultCodeTruthy(code)) {
        throw std::invalid_argument(
            "Gameplay Ability failure code must not be empty");
    }
    return std::make_shared<GameplayAbilityResult>(false, std::move(code),
                                                   std::move(data));
}

GameplayEventData::GameplayEventData(RuntimeValue eventInstigator,
                                     RuntimeValue eventTarget, std::string tag,
                                     RuntimeIdentityPtr eventPayload)
    : instigator(std::move(eventInstigator)),
      target(std::move(eventTarget)),
      eventTag(std::move(tag)),
      payload(std::move(eventPayload)) {
    if (payload == nullptr) {
        payload = runtimeMap();
    }
}

std::shared_ptr<GameplayAbilityResult> GameplayAbility::canActivate(
    const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
    const std::shared_ptr<GameplayEventData>&) {
    for (const std::string& tag : requiredTags) {
        if (!abilitySystem->hasMatchingGameplayTag(tag)) {
            return GameplayAbilityResult::Failure(
                RuntimeValue("MissingRequiredTag"),
                runtimeMap({{"tag", RuntimeValue(tag)}}));
        }
    }
    for (const std::string& tag : blockedTags) {
        if (abilitySystem->hasMatchingGameplayTag(tag)) {
            return GameplayAbilityResult::Failure(
                RuntimeValue("BlockedByTag"),
                runtimeMap({{"tag", RuntimeValue(tag)}}));
        }
    }
    return GameplayAbilityResult::Success();
}

std::shared_ptr<GameplayAbilityResult> GameplayAbility::calculate(
    const std::shared_ptr<AbilitySystemComponent>&,
    const std::shared_ptr<GameplayEventData>&) {
    return GameplayAbilityResult::Success();
}

std::shared_ptr<GameplayAbilityResult> GameplayAbility::activate(
    const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
    const std::shared_ptr<GameplayEventData>& eventData) {
    return calculate(abilitySystem, eventData);
}

GameplayEffect::GameplayEffect() : data(runtimeMap()) {}

GameplayEffectSpec::GameplayEffectSpec(
    std::shared_ptr<GameplayEffect> gameplayEffect,
    std::shared_ptr<GameplayEventData> gameplayEvent, int stackCount,
    RuntimeValue source)
    : effect(std::move(gameplayEffect)),
      eventData(std::move(gameplayEvent)),
      stacks(stackCount),
      sourceKey(std::move(source)) {
    if (effect == nullptr) {
        throw std::invalid_argument("Gameplay Effect is required");
    }
    if (stacks <= 0) {
        throw std::invalid_argument("Gameplay Effect stacks must be positive");
    }
}

GameplayAbilitySpec::GameplayAbilitySpec(
    std::shared_ptr<GameplayAbility> gameplayAbility, RuntimeValue source,
    int order)
    : ability(std::move(gameplayAbility)),
      sourceKey(std::move(source)),
      grantOrder(order) {
    if (ability == nullptr) {
        throw std::invalid_argument("Gameplay Ability is required");
    }
}

ActiveGameplayEffect::ActiveGameplayEffect(
    int effectHandle, std::shared_ptr<GameplayEffectSpec> effectSpec, int order)
    : handle(effectHandle),
      spec(std::move(effectSpec)),
      stacks(spec == nullptr ? 1 : spec->stacks),
      applicationOrder(order) {
    if (spec == nullptr) {
        throw std::invalid_argument("Gameplay Effect Spec is required");
    }
}

RuntimeValue AttributeSet::selfValue() const {
    std::shared_ptr<RuntimeObject> owner = runtimeOwner();
    if (owner == nullptr) {
        std::shared_ptr<const RuntimeObject> constOwner =
            weak_from_this().lock();
        owner = std::const_pointer_cast<RuntimeObject>(std::move(constOwner));
    }
    if (owner == nullptr) {
        throw std::logic_error("Attribute Set has no runtime owner");
    }
    return RuntimeValue(std::move(owner));
}

void AttributeSet::initialize(const RuntimeValue::Map& values) {
    const RuntimeValue self = selfValue();
    const RuntimeValue type =
        ludork::engine::runtime_services::invokeFirst("reflect.type", {self});
    const RuntimeValue rawNames = ludork::engine::runtime_services::invokeFirst(
        "reflect.get", {type, RuntimeValue("ATTRIBUTE_NAMES")});
    const RuntimeValue rawSchema =
        ludork::engine::runtime_services::invokeFirst(
            "reflect.get", {type, RuntimeValue("SCHEMA")});
    const RuntimeValue::Array* names = rawNames.getIf<RuntimeValue::Array>();
    const RuntimeValue::Map* schema = rawSchema.getIf<RuntimeValue::Map>();
    if (names == nullptr || schema == nullptr) {
        throw std::invalid_argument(
            "Attribute Set type must declare ATTRIBUTE_NAMES and SCHEMA");
    }

    attributeNames_.clear();
    attributeNames_.reserve(names->size());
    schema_ = *schema;
    for (const RuntimeValue& rawName : *names) {
        const std::string* name = rawName.getIf<std::string>();
        if (name == nullptr || name->empty()) {
            throw std::invalid_argument(
                "Attribute Set names must be non-empty strings");
        }
        const auto schemaIt = schema_.find(*name);
        if (schemaIt == schema_.end()) {
            throw std::invalid_argument("Attribute schema is missing for " +
                                        *name);
        }
        const RuntimeValue::Map* entry =
            schemaIt->second.getIf<RuntimeValue::Map>();
        if (entry == nullptr) {
            throw std::invalid_argument("Attribute schema must be a table: " +
                                        *name);
        }
        const auto valueIt = values.find(*name);
        const RuntimeValue* selected =
            valueIt == values.end() || valueIt->second.isNil()
                ? nullptr
                : &valueIt->second;
        if (selected == nullptr) {
            const auto defaultIt = entry->find("default");
            if (defaultIt != entry->end()) {
                selected = &defaultIt->second;
            }
        }
        const RuntimeValue value =
            selected == nullptr ? RuntimeValue() : cloneRuntimeValue(*selected);
        ludork::engine::runtime_services::invokeVoid(
            "reflect.set", {self, RuntimeValue(*name), value});
        attributeNames_.push_back(*name);
    }

    const auto idIt = values.find("ID");
    RuntimeValue id = idIt == values.end() || idIt->second.isNil()
                          ? ludork::engine::runtime_services::invokeFirst(
                                "reflect.get", {type, RuntimeValue("ID")})
                          : idIt->second;
    if (id.isNil()) {
        id = RuntimeValue("");
    }
    ludork::engine::runtime_services::invokeVoid(
        "reflect.set", {self, RuntimeValue("ID"), cloneRuntimeValue(id)});
}

std::vector<std::string> AttributeSet::getAttributeNames() const {
    return attributeNames_;
}

RuntimeValue AttributeSet::getAttributeSchema(const std::string& name) const {
    const auto iterator = schema_.find(name);
    return iterator == schema_.end() ? RuntimeValue() : iterator->second;
}

RuntimeValue AttributeSet::getAttributeValue(const std::string& name) const {
    return ludork::engine::runtime_services::invokeFirst(
        "reflect.get", {selfValue(), RuntimeValue(name)});
}

void AttributeSet::setAttributeValue(const std::string& name,
                                     const RuntimeValue& value) {
    ludork::engine::runtime_services::invokeVoid(
        "reflect.set", {selfValue(), RuntimeValue(name), value});
}

std::string AttributeSet::getAttributeType(const std::string& name) const {
    const auto iterator = schema_.find(name);
    if (iterator == schema_.end()) {
        throw std::invalid_argument("Unknown attribute schema: " + name);
    }
    const RuntimeValue::Map* schema =
        iterator->second.getIf<RuntimeValue::Map>();
    if (schema == nullptr) {
        throw std::invalid_argument("Attribute schema must be a table: " +
                                    name);
    }
    const auto type = schema->find("type");
    const std::string* result =
        type == schema->end() ? nullptr : type->second.getIf<std::string>();
    if (result == nullptr) {
        throw std::invalid_argument("Attribute schema type is missing: " +
                                    name);
    }
    return *result;
}
