#include <GlobalFunctions/Components.hpp>

#include <Runtime/Components/ComponentRuntime.hpp>

RuntimeValue cloneComponentValue(const RuntimeValue& value,
                                 const RuntimeValue& valueType,
                                 const RuntimeValue& declaringModule) {
    return ludork::runtime::components::cloneComponentValue(value, valueType,
                                                            declaringModule);
}

RuntimeValue cloneComponentFieldValue(const RuntimeValue& componentType,
                                      const std::string& fieldName,
                                      const RuntimeValue& value) {
    return ludork::runtime::components::cloneComponentFieldValue(
        componentType, fieldName, value);
}

bool isComponentType(const RuntimeValue& value) {
    return ludork::runtime::components::isComponentType(value);
}

RuntimeValue::Map getComponentTypes(const RuntimeValue& classValue) {
    return ludork::runtime::components::getComponentTypes(classValue);
}

RuntimeValue::Map getComponentFieldDefaults(const RuntimeValue& componentType) {
    return ludork::runtime::components::getComponentFieldDefaults(
        componentType);
}

std::unordered_map<std::string, std::string> getComponentFieldMap(
    const RuntimeValue& classValue) {
    return ludork::runtime::components::getComponentFieldMap(classValue);
}

RuntimeValue componentFromData(const RuntimeValue& componentType,
                               const RuntimeValue& data) {
    return ludork::runtime::components::componentFromData(componentType, data);
}

RuntimeValue::Map componentToData(const RuntimeValue& value) {
    return ludork::runtime::components::componentToData(value);
}

std::tuple<RuntimeValue, RuntimeValue, RuntimeValue> getComponentFieldTarget(
    const RuntimeValue& object, const std::string& fieldName) {
    return ludork::runtime::components::getComponentFieldTarget(object,
                                                                fieldName);
}

RuntimeValue getComponentFieldValue(const RuntimeValue& object,
                                    const std::string& fieldName,
                                    const RuntimeValue& defaultValue) {
    return ludork::runtime::components::getComponentFieldValue(
        object, fieldName, defaultValue);
}

bool setComponentFieldValue(const RuntimeValue& object,
                            const std::string& fieldName,
                            const RuntimeValue& value) {
    return ludork::runtime::components::setComponentFieldValue(
        object, fieldName, value);
}

bool isBlankComponentValue(const RuntimeValue& value) {
    return ludork::runtime::components::isBlankComponentValue(value);
}

void mergeComponentDefaults(const RuntimeValue& object) {
    ludork::runtime::components::mergeComponentDefaults(object);
}

void normaliseInstanceComponents(const RuntimeValue& object) {
    ludork::runtime::components::normaliseInstanceComponents(object);
}

RuntimeValue::Array attachInstanceComponents(const RuntimeValue& object) {
    return ludork::runtime::components::attachInstanceComponents(object);
}
