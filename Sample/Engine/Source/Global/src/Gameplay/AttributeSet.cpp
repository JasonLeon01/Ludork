#include <Gameplay/AttributeSet.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include <Runtime/RuntimeObject.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

RuntimeValue cloneRuntimeValue(const RuntimeValue& value) {
    return runtimeReflection().clone(value);
}

}  // namespace

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
    schema_ = schema->toMap();
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
        std::optional<RuntimeMapView> entry =
            RuntimeValueView(schemaIt->second).map();
        if (!entry) {
            throw std::invalid_argument("Attribute schema must be a table: " +
                                        *name);
        }
        const auto valueIt = values.find(*name);
        std::optional<RuntimeValueView> selected;
        if (valueIt != values.end() && !valueIt->second.isNil()) {
            selected = RuntimeValueView(valueIt->second);
        }
        if (!selected) {
            selected = entry->find("default");
        }
        const RuntimeValue value =
            !selected ? RuntimeValue() : cloneRuntimeValue(selected->toValue());
        runtimeReflection().set(ludork::runtime::reference::intern(self), *name,
                                value);
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

RuntimeValue AttributeSet::getAttributeSchema(const std::string& name) const {
    const auto iterator = schema_.find(name);
    return iterator == schema_.end() ? RuntimeValue() : iterator->second;
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

std::string AttributeSet::getAttributeType(const std::string& name) const {
    const auto iterator = schema_.find(name);
    if (iterator == schema_.end()) {
        throw std::invalid_argument("Unknown attribute schema: " + name);
    }
    std::optional<RuntimeMapView> schema =
        RuntimeValueView(iterator->second).map();
    if (!schema) {
        throw std::invalid_argument("Attribute schema must be a table: " +
                                    name);
    }
    const auto type = schema->find("type");
    const std::string* result = !type ? nullptr : type->getIf<std::string>();
    if (result == nullptr) {
        throw std::invalid_argument("Attribute schema type is missing: " +
                                    name);
    }
    return *result;
}
