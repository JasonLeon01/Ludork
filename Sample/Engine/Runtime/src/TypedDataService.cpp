#include <Runtime/TypedDataService.hpp>

#include <Runtime/MetadataRuntime.hpp>

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

}  // namespace

bool TypedDataService::isContainerValueType(
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

bool TypedDataService::isStandardValueType(
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

bool TypedDataService::shouldEvalValueType(
    const RuntimeValue& valueType) const {
    const std::string* text = valueType.getIf<std::string>();
    return (text != nullptr && *text == "any") ||
           !isStandardValueType(valueType);
}

std::optional<std::string> TypedDataService::getClassModulePath(
    const RuntimeValue& classReference) const {
    return metadataRuntime().classModulePath(classReference);
}

std::pair<RuntimeValue, RuntimeValue> TypedDataService::getClassTypeMetadata(
    const RuntimeValue& classReference) const {
    return metadataRuntime().classTypeMetadata(classReference);
}

RuntimeValue TypedDataService::getAttrMetadata(
    const RuntimeValue& owner) const {
    RuntimeValue value = metadataRuntime().attrMetadata(owner);
    return value.isNil() ? RuntimeValue(RuntimeValue::Map{}) : value;
}

RuntimeValue TypedDataService::resolveAttrMetadata(
    const RuntimeValue& owner, const std::string& key) const {
    return metadataRuntime().resolveAttrMetadata(owner, key);
}

RuntimeValue TypedDataService::resolveAttrValueType(
    const RuntimeValue& owner, const std::string& key) const {
    return metadataRuntime().resolveAttrValueType(owner, key);
}

std::pair<RuntimeValue, RuntimeValue> TypedDataService::resolveConfigVar(
    const RuntimeValue& owner, const std::string& key) const {
    return metadataRuntime().resolveConfigVar(owner, key);
}

std::pair<RuntimeValue, RuntimeValue> TypedDataService::resolveMemberMetadata(
    const RuntimeValue& owner, const std::string& key) const {
    return metadataRuntime().resolveMemberMetadata(owner, key);
}

RuntimeValue TypedDataService::evalDataExpression(
    const RuntimeValue& value, const RuntimeValue::Map& environment) const {
    const std::string* expression = value.getIf<std::string>();
    if (expression == nullptr) {
        return value;
    }
    const std::string text = trim(*expression);
    if (text.empty()) {
        return RuntimeValue();
    }
    return metadataRuntime().evaluateExpression(RuntimeValue(*expression),
                                                environment);
}

RuntimeValue TypedDataService::coerceStandardValue(
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

RuntimeValue TypedDataService::resolveMetadataType(
    const RuntimeValue& typeReference,
    const std::string& declaringModule) const {
    return metadataRuntime().resolveType(typeReference, declaringModule);
}

std::string TypedDataService::metadataTypeName(
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

RuntimeValue TypedDataService::constructTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule) const {
    return metadataRuntime().constructTypedValue(value, valueType,
                                                 declaringModule);
}

RuntimeValue TypedDataService::resolveTypedDataValue(
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

RuntimeValue TypedDataService::unwrapOptional(
    const RuntimeValue& valueType) const {
    const RuntimeValue* optional = mapValue(valueType, "optional");
    return optional == nullptr ? valueType : *optional;
}

RuntimeValue TypedDataService::coerceUnionValue(
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

bool TypedDataService::matchesType(const RuntimeValue& value,
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

RuntimeValue TypedDataService::coerceBool(const RuntimeValue& value) const {
    return value;
}

RuntimeValue TypedDataService::coerceInteger(const RuntimeValue& value) const {
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

RuntimeValue TypedDataService::coerceFloat(const RuntimeValue& value) const {
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

RuntimeValue TypedDataService::coerceContainer(
    const RuntimeValue& value) const {
    return value.getIf<std::string>() == nullptr ? value
                                                 : evalDataExpression(value);
}

TypedDataService& typedDataService() {
    static TypedDataService service;
    return service;
}

RuntimeValue dataValueGetClassModulePath(const RuntimeValue& classReference) {
    const std::optional<std::string> module =
        typedDataService().getClassModulePath(classReference);
    return module.has_value() ? RuntimeValue(*module) : RuntimeValue();
}

std::pair<RuntimeValue, RuntimeValue> dataValueGetClassTypeMetadata(
    const RuntimeValue& classReference) {
    return typedDataService().getClassTypeMetadata(classReference);
}

RuntimeValue dataValueGetAttrMetadata(const RuntimeValue& owner) {
    return typedDataService().getAttrMetadata(owner);
}

RuntimeValue dataValueResolveAttrMetadata(const RuntimeValue& owner,
                                          const std::string& key) {
    return typedDataService().resolveAttrMetadata(owner, key);
}

RuntimeValue dataValueResolveAttrValueType(const RuntimeValue& owner,
                                           const std::string& key) {
    return typedDataService().resolveAttrValueType(owner, key);
}

std::pair<RuntimeValue, RuntimeValue> dataValueResolveConfigVar(
    const RuntimeValue& owner, const std::string& key) {
    return typedDataService().resolveConfigVar(owner, key);
}

std::pair<RuntimeValue, RuntimeValue> dataValueResolveMemberMetadata(
    const RuntimeValue& owner, const std::string& key) {
    return typedDataService().resolveMemberMetadata(owner, key);
}

bool dataValueIsContainerValueType(const RuntimeValue& valueType) {
    return typedDataService().isContainerValueType(valueType);
}

bool dataValueIsStandardValueType(const RuntimeValue& valueType) {
    return typedDataService().isStandardValueType(valueType);
}

bool dataValueShouldEvalValueType(const RuntimeValue& valueType) {
    return typedDataService().shouldEvalValueType(valueType);
}

RuntimeValue dataValueEvalDataExpression(const RuntimeValue& value,
                                         const RuntimeValue::Map& environment) {
    return typedDataService().evalDataExpression(value, environment);
}

RuntimeValue dataValueCoerceStandardValue(const RuntimeValue& value,
                                          const RuntimeValue& valueType) {
    return typedDataService().coerceStandardValue(value, valueType);
}

RuntimeValue dataValueResolveMetadataType(const RuntimeValue& typeReference,
                                          const std::string& declaringModule) {
    return typedDataService().resolveMetadataType(typeReference,
                                                  declaringModule);
}

std::string dataValueMetadataTypeName(const RuntimeValue& typeReference) {
    return typedDataService().metadataTypeName(typeReference);
}

RuntimeValue dataValueConstructTypedValue(const RuntimeValue& value,
                                          const RuntimeValue& valueType,
                                          const std::string& declaringModule) {
    return typedDataService().constructTypedValue(value, valueType,
                                                  declaringModule);
}

RuntimeValue dataValueResolveTypedDataValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const RuntimeValue::Map& environment, const std::string& declaringModule) {
    return typedDataService().resolveTypedDataValue(
        value, valueType, environment, declaringModule);
}

RuntimeValue::Map getConfigVars(const RuntimeValue& meta) {
    return metadataRuntime().configVars(meta);
}
