#include <Utils/DataValue.hpp>

#include <Runtime/RuntimeValueServices.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

std::string trim(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char character) {
                                            return std::isspace(character) != 0;
                                        });
    if (first == value.end()) {
        return std::string();
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char character) {
                                           return std::isspace(character) != 0;
                                       });
    return std::string(first, last.base());
}

bool endsWithArray(const std::string& value) {
    return value.size() >= 2 && value.ends_with("[]");
}

const RuntimeValue* mapValue(const RuntimeValue& value,
                             const std::string& key) {
    const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>();
    if (map == nullptr) {
        return nullptr;
    }
    const auto iterator = map->find(key);
    return iterator == map->end() ? nullptr : &iterator->second;
}

std::string scalarString(const RuntimeValue& value) {
    if (const std::string* text = value.getIf<std::string>()) {
        return *text;
    }
    if (const bool* boolean = value.getIf<bool>()) {
        return *boolean ? "true" : "false";
    }
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return std::to_string(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        std::ostringstream stream;
        stream << std::setprecision(15) << *number;
        return stream.str();
    }
    return value.typeName();
}

bool parseInteger(const std::string& text, std::int64_t& result) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const std::from_chars_result parsed = std::from_chars(begin, end, result);
    if (parsed.ec == std::errc() && parsed.ptr == end) {
        return true;
    }
    char* floatEnd = nullptr;
    const double number = std::strtod(text.c_str(), &floatEnd);
    if (floatEnd == text.c_str() + text.size() && std::isfinite(number)) {
        result = static_cast<std::int64_t>(number);
        return true;
    }
    return false;
}

bool parseFloat(const std::string& text, double& result) {
    char* end = nullptr;
    result = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(result);
}

void addConfigVarReference(RuntimeValue::Map& result, const std::string& name,
                           const RuntimeValue& reference) {
    if (name.empty()) {
        return;
    }
    if (const std::string* text = reference.getIf<std::string>()) {
        if (text->empty()) {
            return;
        }
        const std::size_t separator = text->find('.');
        if (separator == std::string::npos) {
            result[name] = RuntimeValue(
                RuntimeValue::Array{RuntimeValue(*text), RuntimeValue(name)});
            return;
        }
        const std::string configName = text->substr(0, separator);
        const std::string settingName = text->substr(separator + 1);
        if (!configName.empty() && !settingName.empty()) {
            result[name] = RuntimeValue(RuntimeValue::Array{
                RuntimeValue(configName), RuntimeValue(settingName)});
        }
        return;
    }
    const RuntimeValue::Array* values = reference.getIf<RuntimeValue::Array>();
    if (values == nullptr || values->size() < 2) {
        return;
    }
    const std::string* configName = (*values)[0].getIf<std::string>();
    const std::string* settingName = (*values)[1].getIf<std::string>();
    if (configName == nullptr || configName->empty() ||
        settingName == nullptr || settingName->empty()) {
        return;
    }
    result[name] = RuntimeValue(RuntimeValue::Array{
        RuntimeValue(*configName), RuntimeValue(*settingName)});
}

void addConfigVarItem(RuntimeValue::Map& result, const RuntimeValue& item) {
    if (const std::string* name = item.getIf<std::string>()) {
        if (!name->empty()) {
            result[*name] = RuntimeValue(RuntimeValue::Array{
                RuntimeValue(std::string("System")), RuntimeValue(*name)});
        }
        return;
    }
    const RuntimeValue::Array* values = item.getIf<RuntimeValue::Array>();
    if (values == nullptr || values->size() < 2) {
        return;
    }
    const std::string* name = (*values)[0].getIf<std::string>();
    if (name == nullptr || name->empty()) {
        return;
    }
    if (values->size() >= 3) {
        addConfigVarReference(
            result, *name,
            RuntimeValue(RuntimeValue::Array{(*values)[1], (*values)[2]}));
        return;
    }
    addConfigVarReference(result, *name, (*values)[1]);
}

}  // namespace

bool DataValueService::isContainerValueType(
    const RuntimeValue& valueType) const {
    if (const std::string* text = valueType.getIf<std::string>()) {
        return *text == "table" || *text == "list" || *text == "dict" ||
               *text == "Pair" || *text == "pair" || endsWithArray(*text) ||
               text->starts_with("List[") || text->starts_with("Dict[") ||
               text->starts_with("Set[") || text->starts_with("Tuple[");
    }
    const RuntimeValue::Array* reference =
        valueType.getIf<RuntimeValue::Array>();
    if (reference != nullptr && reference->size() >= 2) {
        const std::string* name = (*reference)[1].getIf<std::string>();
        return name != nullptr && endsWithArray(*name);
    }
    return false;
}

bool DataValueService::isStandardValueType(
    const RuntimeValue& valueType) const {
    if (valueType.isNil()) {
        return true;
    }
    if (isContainerValueType(valueType)) {
        return true;
    }
    if (mapValue(valueType, "optional") != nullptr) {
        return isStandardValueType(unwrapOptional(valueType));
    }
    if (const RuntimeValue* unionValue = mapValue(valueType, "union")) {
        const RuntimeValue::Array* arguments =
            unionValue->getIf<RuntimeValue::Array>();
        if (arguments == nullptr || arguments->empty()) {
            return false;
        }
        return std::all_of(arguments->begin(), arguments->end(),
                           [this](const RuntimeValue& argument) {
                               return isStandardValueType(argument);
                           });
    }
    const std::string* text = valueType.getIf<std::string>();
    if (text == nullptr) {
        return false;
    }
    const std::string type = lower(*text);
    return type == "nil" || type == "any" || type == "bool" ||
           type == "number" || type == "int" || type == "float" ||
           type == "string";
}

bool DataValueService::shouldEvalValueType(
    const RuntimeValue& valueType) const {
    const std::string* text = valueType.getIf<std::string>();
    return (text != nullptr && *text == "any") ||
           !isStandardValueType(valueType);
}

RuntimeValue DataValueService::getClassModulePath(
    const RuntimeValue& classReference) const {
    return ludork::engine::runtime_services::firstResult(
        resolveDynamic("getClassModulePath", {classReference}));
}

std::pair<RuntimeValue, RuntimeValue> DataValueService::getClassTypeMetadata(
    const RuntimeValue& classReference) const {
    const std::vector<RuntimeValue> values =
        resolveDynamic("getClassTypeMetadata", {classReference});
    return {values.empty() ? RuntimeValue() : values[0],
            values.size() < 2 ? RuntimeValue() : values[1]};
}

RuntimeValue DataValueService::getAttrMetadata(
    const RuntimeValue& owner) const {
    RuntimeValue value = ludork::engine::runtime_services::firstResult(
        resolveDynamic("getAttrMetadata", {owner}));
    return value.isNil() ? RuntimeValue(RuntimeValue::Map{}) : value;
}

RuntimeValue DataValueService::resolveAttrMetadata(
    const RuntimeValue& owner, const std::string& key) const {
    return ludork::engine::runtime_services::firstResult(
        resolveDynamic("resolveAttrMetadata", {owner, RuntimeValue(key)}));
}

RuntimeValue DataValueService::resolveAttrValueType(
    const RuntimeValue& owner, const std::string& key) const {
    return ludork::engine::runtime_services::firstResult(
        resolveDynamic("resolveAttrValueType", {owner, RuntimeValue(key)}));
}

std::pair<RuntimeValue, RuntimeValue> DataValueService::resolveConfigVar(
    const RuntimeValue& owner, const std::string& key) const {
    const std::vector<RuntimeValue> values =
        resolveDynamic("resolveConfigVar", {owner, RuntimeValue(key)});
    return {values.empty() ? RuntimeValue() : values[0],
            values.size() < 2 ? RuntimeValue() : values[1]};
}

std::pair<RuntimeValue, RuntimeValue> DataValueService::resolveMemberMetadata(
    const RuntimeValue& owner, const std::string& key) const {
    const std::vector<RuntimeValue> values =
        resolveDynamic("resolveMemberMetadata", {owner, RuntimeValue(key)});
    return {values.empty() ? RuntimeValue() : values[0],
            values.size() < 2 ? RuntimeValue() : values[1]};
}

RuntimeValue DataValueService::evalDataExpression(
    const RuntimeValue& value, const RuntimeValue::Map& environment) const {
    const std::string* expression = value.getIf<std::string>();
    if (expression == nullptr) {
        return value;
    }
    const std::string text = trim(*expression);
    if (text.empty()) {
        return RuntimeValue();
    }
    const std::vector<RuntimeValue> values =
        resolveDynamic("evalDataExpression",
                       {RuntimeValue(*expression), RuntimeValue(environment)});
    if (!values.empty()) {
        return values.front();
    }
    std::int64_t integer = 0;
    if (parseInteger(text, integer)) {
        return RuntimeValue(integer);
    }
    double number = 0.0;
    if (parseFloat(text, number)) {
        return RuntimeValue(number);
    }
    return value;
}

RuntimeValue DataValueService::coerceStandardValue(
    const RuntimeValue& value, const RuntimeValue& valueType) const {
    if (value.isNil()) {
        return value;
    }
    const RuntimeValue unwrapped = unwrapOptional(valueType);
    if (const RuntimeValue* unionValue = mapValue(unwrapped, "union")) {
        if (const RuntimeValue::Array* arguments =
                unionValue->getIf<RuntimeValue::Array>()) {
            return coerceUnionValue(value, *arguments);
        }
    }
    if (const std::string* text = unwrapped.getIf<std::string>()) {
        const std::string type = lower(*text);
        if (type == "string") {
            return RuntimeValue(scalarString(value));
        }
        if (type == "bool") {
            return coerceBool(value);
        }
        if (type == "int") {
            return coerceInteger(value);
        }
        if (type == "number" || type == "float") {
            return coerceFloat(value);
        }
        if (type == "nil") {
            return RuntimeValue();
        }
    }
    if (isContainerValueType(unwrapped)) {
        return coerceContainer(value);
    }
    return value.getIf<std::string>() == nullptr ? value
                                                 : evalDataExpression(value);
}

RuntimeValue DataValueService::resolveMetadataType(
    const RuntimeValue& typeReference,
    const std::string& declaringModule) const {
    return ludork::engine::runtime_services::firstResult(resolveDynamic(
        "resolveMetadataType", {typeReference, RuntimeValue(declaringModule)}));
}

std::string DataValueService::metadataTypeName(
    const RuntimeValue& typeReference) const {
    if (const RuntimeValue::Array* reference =
            typeReference.getIf<RuntimeValue::Array>()) {
        if (reference->size() >= 2) {
            return scalarString((*reference)[0]) + "." +
                   scalarString((*reference)[1]);
        }
    }
    return scalarString(typeReference);
}

RuntimeValue DataValueService::constructTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule) const {
    const std::vector<RuntimeValue> values =
        resolveDynamic("constructTypedValue",
                       {value, valueType, RuntimeValue(declaringModule)});
    return values.empty() ? value : values.front();
}

RuntimeValue DataValueService::resolveTypedDataValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const RuntimeValue::Map& environment,
    const std::string& declaringModule) const {
    RuntimeValue resolved = value;
    if (shouldEvalValueType(valueType) &&
        value.getIf<std::string>() != nullptr) {
        resolved = evalDataExpression(value, environment);
    }
    const RuntimeValue unwrapped = unwrapOptional(valueType);
    if (isStandardValueType(unwrapped) ||
        mapValue(unwrapped, "union") != nullptr) {
        return coerceStandardValue(resolved, unwrapped);
    }
    return constructTypedValue(resolved, unwrapped, declaringModule);
}

std::vector<RuntimeValue> DataValueService::resolveDynamic(
    const std::string& operation, std::vector<RuntimeValue> arguments) const {
    return resolveRuntime(operation, arguments);
}

RuntimeValue DataValueService::unwrapOptional(
    const RuntimeValue& valueType) const {
    const RuntimeValue* optional = mapValue(valueType, "optional");
    return optional == nullptr ? valueType : *optional;
}

RuntimeValue DataValueService::coerceUnionValue(
    const RuntimeValue& value, const RuntimeValue::Array& arguments) const {
    RuntimeValue last = value;
    for (const RuntimeValue& argument : arguments) {
        const std::string* name = argument.getIf<std::string>();
        if (name != nullptr && *name == "nil") {
            continue;
        }
        RuntimeValue coerced = coerceStandardValue(value, argument);
        if (matchesType(coerced, argument)) {
            return coerced;
        }
        last = std::move(coerced);
    }
    return last;
}

bool DataValueService::matchesType(const RuntimeValue& value,
                                   const RuntimeValue& valueType) const {
    const std::string* name = valueType.getIf<std::string>();
    if (name == nullptr) {
        return false;
    }
    const std::string type = lower(*name);
    if (type == "any") {
        return true;
    }
    if (type == "nil") {
        return value.isNil();
    }
    if (type == "bool") {
        return value.getIf<bool>() != nullptr;
    }
    if (type == "int") {
        return value.getIf<std::int64_t>() != nullptr;
    }
    if (type == "number" || type == "float") {
        return value.getIf<double>() != nullptr ||
               value.getIf<std::int64_t>() != nullptr;
    }
    if (type == "string") {
        return value.getIf<std::string>() != nullptr;
    }
    if (type == "list" || type == "table" || type == "pair") {
        return value.getIf<RuntimeValue::Array>() != nullptr ||
               value.getIf<RuntimeValue::Map>() != nullptr;
    }
    if (type == "dict") {
        return value.getIf<RuntimeValue::Map>() != nullptr;
    }
    return false;
}

RuntimeValue DataValueService::coerceBool(const RuntimeValue& value) const {
    return value;
}

RuntimeValue DataValueService::coerceInteger(const RuntimeValue& value) const {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return RuntimeValue(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        return RuntimeValue(static_cast<std::int64_t>(*number));
    }
    if (const bool* boolean = value.getIf<bool>()) {
        return RuntimeValue(static_cast<std::int64_t>(*boolean ? 1 : 0));
    }
    if (const std::string* source = value.getIf<std::string>()) {
        const std::string text = trim(*source);
        std::int64_t integer = 0;
        if (parseInteger(text, integer)) {
            return RuntimeValue(integer);
        }
        RuntimeValue literal = evalDataExpression(value);
        if (literal.getIf<std::int64_t>() != nullptr ||
            literal.getIf<double>() != nullptr) {
            return coerceInteger(literal);
        }
    }
    return value;
}

RuntimeValue DataValueService::coerceFloat(const RuntimeValue& value) const {
    if (const double* number = value.getIf<double>()) {
        return RuntimeValue(*number);
    }
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return RuntimeValue(static_cast<double>(*integer));
    }
    if (const bool* boolean = value.getIf<bool>()) {
        return RuntimeValue(*boolean ? 1.0 : 0.0);
    }
    if (const std::string* source = value.getIf<std::string>()) {
        const std::string text = trim(*source);
        double number = 0.0;
        if (parseFloat(text, number)) {
            return RuntimeValue(number);
        }
        RuntimeValue literal = evalDataExpression(value);
        if (literal.getIf<std::int64_t>() != nullptr ||
            literal.getIf<double>() != nullptr) {
            return coerceFloat(literal);
        }
    }
    return value;
}

RuntimeValue DataValueService::coerceContainer(
    const RuntimeValue& value) const {
    return value.getIf<std::string>() == nullptr ? value
                                                 : evalDataExpression(value);
}

DataValueService& dataValueService() {
    static DataValueService service;
    return service;
}

RuntimeValue dataValueGetClassModulePath(const RuntimeValue& classReference) {
    return dataValueService().getClassModulePath(classReference);
}

std::pair<RuntimeValue, RuntimeValue> dataValueGetClassTypeMetadata(
    const RuntimeValue& classReference) {
    return dataValueService().getClassTypeMetadata(classReference);
}

RuntimeValue dataValueGetAttrMetadata(const RuntimeValue& owner) {
    return dataValueService().getAttrMetadata(owner);
}

RuntimeValue dataValueResolveAttrMetadata(const RuntimeValue& owner,
                                          const std::string& key) {
    return dataValueService().resolveAttrMetadata(owner, key);
}

RuntimeValue dataValueResolveAttrValueType(const RuntimeValue& owner,
                                           const std::string& key) {
    return dataValueService().resolveAttrValueType(owner, key);
}

std::pair<RuntimeValue, RuntimeValue> dataValueResolveConfigVar(
    const RuntimeValue& owner, const std::string& key) {
    return dataValueService().resolveConfigVar(owner, key);
}

std::pair<RuntimeValue, RuntimeValue> dataValueResolveMemberMetadata(
    const RuntimeValue& owner, const std::string& key) {
    return dataValueService().resolveMemberMetadata(owner, key);
}

bool dataValueIsContainerValueType(const RuntimeValue& valueType) {
    return dataValueService().isContainerValueType(valueType);
}

bool dataValueIsStandardValueType(const RuntimeValue& valueType) {
    return dataValueService().isStandardValueType(valueType);
}

bool dataValueShouldEvalValueType(const RuntimeValue& valueType) {
    return dataValueService().shouldEvalValueType(valueType);
}

RuntimeValue dataValueEvalDataExpression(const RuntimeValue& value,
                                         const RuntimeValue::Map& environment) {
    return dataValueService().evalDataExpression(value, environment);
}

RuntimeValue dataValueCoerceStandardValue(const RuntimeValue& value,
                                          const RuntimeValue& valueType) {
    return dataValueService().coerceStandardValue(value, valueType);
}

RuntimeValue dataValueResolveMetadataType(const RuntimeValue& typeReference,
                                          const std::string& declaringModule) {
    return dataValueService().resolveMetadataType(typeReference,
                                                  declaringModule);
}

std::string dataValueMetadataTypeName(const RuntimeValue& typeReference) {
    return dataValueService().metadataTypeName(typeReference);
}

RuntimeValue dataValueConstructTypedValue(const RuntimeValue& value,
                                          const RuntimeValue& valueType,
                                          const std::string& declaringModule) {
    return dataValueService().constructTypedValue(value, valueType,
                                                  declaringModule);
}

RuntimeValue dataValueResolveTypedDataValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const RuntimeValue::Map& environment, const std::string& declaringModule) {
    return dataValueService().resolveTypedDataValue(
        value, valueType, environment, declaringModule);
}

RuntimeValue::Map getConfigVars(const RuntimeValue& meta) {
    RuntimeValue::Map result;
    const RuntimeValue* rawVars = mapValue(meta, "ConfigVars");
    if (rawVars == nullptr) {
        return result;
    }
    if (const RuntimeValue::Map* values = rawVars->getIf<RuntimeValue::Map>()) {
        for (const auto& [name, reference] : *values) {
            addConfigVarReference(result, name, reference);
        }
        return result;
    }
    if (const RuntimeValue::Array* values =
            rawVars->getIf<RuntimeValue::Array>()) {
        for (const RuntimeValue& item : *values) {
            addConfigVarItem(result, item);
        }
    }
    return result;
}
