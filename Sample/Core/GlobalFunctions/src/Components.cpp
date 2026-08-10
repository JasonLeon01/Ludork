#include <GlobalFunctions/Components.hpp>

#include <Gameplay/Components/ComponentRuntime.hpp>

RuntimeValue cloneComponentValue(const RuntimeValue& value,
                                 const RuntimeValue& valueType,
                                 const RuntimeValue& declaringModule) {
    return ludork::engine::components::cloneComponentValue(value, valueType,
                                                           declaringModule);
}

RuntimeValue cloneComponentFieldValue(const RuntimeValue& componentType,
                                      const std::string& fieldName,
                                      const RuntimeValue& value) {
    return ludork::engine::components::cloneComponentFieldValue(
        componentType, fieldName, value);
}

bool isComponentType(const RuntimeValue& value) {
    return ludork::engine::components::isComponentType(value);
}

RuntimeValue::Map getComponentTypes(const RuntimeValue& classValue) {
    return ludork::engine::components::getComponentTypes(classValue);
}

RuntimeValue::Map getComponentFieldDefaults(const RuntimeValue& componentType) {
    return ludork::engine::components::getComponentFieldDefaults(componentType);
}

std::unordered_map<std::string, std::string> getComponentFieldMap(
    const RuntimeValue& classValue) {
    return ludork::engine::components::getComponentFieldMap(classValue);
}

RuntimeValue componentFromData(const RuntimeValue& componentType,
                               const RuntimeValue& data) {
    return ludork::engine::components::componentFromData(componentType, data);
}

RuntimeValue::Map componentToData(const RuntimeValue& value) {
    return ludork::engine::components::componentToData(value);
}

std::tuple<RuntimeValue, RuntimeValue, RuntimeValue> getComponentFieldTarget(
    const RuntimeValue& object, const std::string& fieldName) {
    return ludork::engine::components::getComponentFieldTarget(object,
                                                               fieldName);
}

RuntimeValue getComponentFieldValue(const RuntimeValue& object,
                                    const std::string& fieldName,
                                    const RuntimeValue& defaultValue) {
    return ludork::engine::components::getComponentFieldValue(object, fieldName,
                                                              defaultValue);
}

bool setComponentFieldValue(const RuntimeValue& object,
                            const std::string& fieldName,
                            const RuntimeValue& value) {
    return ludork::engine::components::setComponentFieldValue(object, fieldName,
                                                              value);
}

bool isBlankComponentValue(const RuntimeValue& value) {
    return ludork::engine::components::isBlankComponentValue(value);
}

void mergeComponentDefaults(const RuntimeValue& object) {
    ludork::engine::components::mergeComponentDefaults(object);
}

void normaliseInstanceComponents(const RuntimeValue& object) {
    ludork::engine::components::normaliseInstanceComponents(object);
}

RuntimeValue::Array attachInstanceComponents(const RuntimeValue& object) {
    return ludork::engine::components::attachInstanceComponents(object);
}
