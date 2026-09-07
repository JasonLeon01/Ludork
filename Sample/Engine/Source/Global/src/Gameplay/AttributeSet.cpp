#include <Gameplay/AttributeSet.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include <Runtime/RuntimeObject.hpp>
#include <Runtime/TypedDataService.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

RuntimeValue cloneRuntimeValue(const RuntimeValue& value) {
    return runtimeReflection().clone(value);
}

}  // namespace

RuntimeValue AttributeSet::AttributeSchema::getDefault() const {
    return defaultValue_;
}

void AttributeSet::AttributeSchema::setDefault(RuntimeValue value) {
    defaultValue_ = std::move(value);
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
    initializeValues(values, false);
}

void AttributeSet::initializeStored(const RuntimeValue::Map& values) {
    initializeValues(values, true);
}

void AttributeSet::initializeValues(const RuntimeValue::Map& values,
                                    bool stored) {
    const RuntimeValue self = selfValue();
    const RuntimeValue type = runtimeReflection().typeOf(self);
    const RuntimeValue rawNames = runtimeReflection().get(
        ludork::runtime::reference::intern(type), "ATTRIBUTE_NAMES");
    const RuntimeValue rawSchema = runtimeReflection().get(
        ludork::runtime::reference::intern(type), "SCHEMA");
    std::optional<RuntimeArrayView> names = RuntimeValueView(rawNames).array();
    std::optional<RuntimeMapView> schema = RuntimeValueView(rawSchema).map();
    if (!names || !schema) {
        throw std::invalid_argument(
            "Attribute Set type must declare ATTRIBUTE_NAMES and SCHEMA");
    }

    attributeNames_.clear();
    attributeNames_.reserve(names->size());
    schema_.clear();
    for (const auto& [name, rawEntry] : *schema) {
        const std::optional<RuntimeMapView> entry = rawEntry.map();
        if (!entry) {
            throw std::invalid_argument("Attribute schema must be a table: " +
                                        name);
        }
        const std::optional<RuntimeValueView> type = entry->find("type");
        if (!type || type->isNil()) {
            throw std::invalid_argument("Attribute schema type is missing: " +
                                        name);
        }
        AttributeSchema parsed;
        parsed.type = type->toData();
        if (const std::optional<RuntimeValueView> defaultValue =
                entry->find("default")) {
            parsed.setDefault(defaultValue->toValue());
        }
        schema_.emplace(name, std::move(parsed));
    }
    for (RuntimeValueView rawName : *names) {
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
        const auto valueIt = values.find(*name);
        const RuntimeValue valueType(schemaIt->second.type);
        const bool provided = valueIt != values.end();
        const RuntimeValue rawValue =
            provided ? valueIt->second : schemaIt->second.getDefault();
        const RuntimeValue value = cloneRuntimeValue(
            provided && !stored ? typedDataService().resolveRuntimeTypedValue(
                                      rawValue, valueType)
                                : typedDataService().resolveTypedDataValue(
                                      rawValue, valueType, {}, {}, false));
        runtimeReflection().setTyped(ludork::runtime::reference::intern(self),
                                     *name, value);
        attributeNames_.push_back(*name);
    }

    const auto idIt = values.find("ID");
    RuntimeValue id = idIt == values.end() || idIt->second.isNil()
                          ? runtimeReflection().get(
                                ludork::runtime::reference::intern(type), "ID")
                          : idIt->second;
    if (id.isNil()) {
        id = RuntimeValue("");
    }
    runtimeReflection().set(ludork::runtime::reference::intern(self), "ID",
                            cloneRuntimeValue(id));
}

std::vector<std::string> AttributeSet::getAttributeNames() const {
    return attributeNames_;
}

std::optional<AttributeSet::AttributeSchema> AttributeSet::getAttributeSchema(
    const std::string& name) const {
    const auto iterator = schema_.find(name);
    return iterator == schema_.end()
               ? std::nullopt
               : std::optional<AttributeSchema>(iterator->second);
}

RuntimeValue AttributeSet::getAttributeValue(const std::string& name) const {
    return runtimeReflection().get(
        ludork::runtime::reference::intern(selfValue()), name);
}

void AttributeSet::setAttributeValue(const std::string& name,
                                     const RuntimeValue& value) {
    runtimeReflection().set(ludork::runtime::reference::intern(selfValue()),
                            name, value);
}

std::optional<AttributeSet::NumericType> AttributeSet::getNumericAttributeType(
    const std::string& name) const {
    const auto iterator = schema_.find(name);
    if (iterator == schema_.end()) {
        throw std::invalid_argument("Unknown attribute schema: " + name);
    }
    const std::string* type = iterator->second.type.getIf<std::string>();
    if (type == nullptr) {
        return std::nullopt;
    }
    if (*type == "int") {
        return NumericType::Integer;
    }
    if (*type == "float") {
        return NumericType::Float;
    }
    return std::nullopt;
}
