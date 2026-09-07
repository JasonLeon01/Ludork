#include <Runtime/TypedDataService.hpp>

#include <Runtime/MetadataRuntime.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeReflection.hpp>

#include "TypedDataSchema.hpp"

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

std::optional<RuntimeValueView> mapValue(RuntimeValueView value,
                                         const std::string& key) {
    const auto map = value.map();
    return map ? map->find(key) : std::nullopt;
}

std::string scalarString(RuntimeValueView value) {
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
    const double truncated = std::trunc(number);
    const double limit = std::ldexp(1.0, 63);
    if (floatEnd != text.c_str() && floatEnd == text.c_str() + text.size() &&
        std::isfinite(truncated) && truncated >= -limit && truncated < limit) {
        result = static_cast<std::int64_t>(truncated);
        return true;
    }
    return false;
}

bool parseFloat(const std::string& text, double& result) {
    char* end = nullptr;
    result = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(result);
}

using ludork::runtime::typed_data_impl::parseSchema;
using ludork::runtime::typed_data_impl::schemaName;
using ludork::runtime::typed_data_impl::schemaValue;
using ludork::runtime::typed_data_impl::TypeSchema;

RuntimeValue snapshotContainer(const RuntimeValue& value) {
    return value.getIf<RuntimeHandle>() == nullptr
               ? value
               : ludork::runtime::reference::snapshot(value);
}

bool matchesRuntime(const RuntimeValue& value, const TypeSchema& type,
                    const std::string& declaringModule) {
    if (type.kind == TypeSchema::Kind::Optional) {
        return value.isNil() ||
               matchesRuntime(value, type.arguments.front(), declaringModule);
    }
    if (type.kind == TypeSchema::Kind::Union) {
        return std::any_of(
            type.arguments.begin(), type.arguments.end(),
            [&value, &declaringModule](const TypeSchema& argument) {
                return matchesRuntime(value, argument, declaringModule);
            });
    }
    if (type.kind == TypeSchema::Kind::List ||
        type.kind == TypeSchema::Kind::Tuple) {
        const std::optional<RuntimeValue::Array> array =
            ludork::runtime::reference::arrayValues(value);
        if (!array || (type.kind == TypeSchema::Kind::Tuple &&
                       array->size() != type.arguments.size())) {
            return false;
        }
        for (std::size_t index = 0; index < array->size(); ++index) {
            const TypeSchema& itemType = type.kind == TypeSchema::Kind::List
                                             ? type.arguments.front()
                                             : type.arguments[index];
            if (!matchesRuntime((*array)[index], itemType, declaringModule)) {
                return false;
            }
        }
        return true;
    }
    if (type.kind == TypeSchema::Kind::Dictionary) {
        const std::optional<RuntimeValue::Map> map =
            ludork::runtime::reference::mapValues(value);
        if (!map) {
            return false;
        }
        for (const auto& [key, item] : *map) {
            if (!matchesRuntime(item, type.arguments.front(),
                                declaringModule)) {
                return false;
            }
        }
        return true;
    }
    if (type.module.empty()) {
        if (type.name == "any") {
            return true;
        }
        if (type.name == "nil") {
            return value.isNil();
        }
        if (type.name == "bool") {
            return value.getIf<bool>() != nullptr;
        }
        if (type.name == "int") {
            return value.getIf<std::int64_t>() != nullptr;
        }
        if (type.name == "float") {
            const double* number = value.getIf<double>();
            return value.getIf<std::int64_t>() != nullptr ||
                   (number != nullptr && std::isfinite(*number));
        }
        if (type.name == "string" || type.name == "file") {
            return value.getIf<std::string>() != nullptr;
        }
        if (type.name == "function" || type.name == "event") {
            return ludork::runtime::reference::isFunction(value);
        }
        if (type.name == "table" || type.name == "Pair" ||
            type.name == "pair") {
            return ludork::runtime::reference::arrayValues(value).has_value() ||
                   ludork::runtime::reference::mapValues(value).has_value();
        }
    }
    const RuntimeValue target =
        metadataRuntime().resolveType(schemaValue(type), declaringModule);
    return !target.isNil() && runtimeReflection().isInstance(value, target);
}

[[noreturn]] void valueTypeError(const std::string& path,
                                 const TypeSchema& type) {
    throw std::invalid_argument(path + " must be " + schemaName(type));
}

RuntimeValue resolveStored(const TypedDataService& service,
                           const RuntimeValue& value, const TypeSchema& type,
                           const RuntimeValue::Map& environment,
                           const std::string& declaringModule,
                           const std::string& path, bool strict,
                           bool evaluateAnyExpressions = true);

RuntimeValue resolveStoredRecord(const TypedDataService& service,
                                 const RuntimeValue& value,
                                 const TypeSchema& type,
                                 const RuntimeValue::Map& environment,
                                 const std::string& declaringModule,
                                 const std::string& path) {
    const RuntimeValue target =
        service.resolveMetadataType(schemaValue(type), declaringModule);
    if (target.isNil()) {
        throw std::invalid_argument(path + ": cannot resolve " +
                                    schemaName(type));
    }
    RuntimeValue result = value;
    const std::optional<RuntimeMapView> map = value.view().map();
    if (map) {
        const RuntimeValue metadata = service.getAttrMetadata(target);
        const RuntimeValue ownedMetadata = snapshotContainer(metadata);
        const std::optional<RuntimeMapView> fields = ownedMetadata.view().map();
        RuntimeValue::Map values = map->toMap();
        if (fields) {
            for (auto& [name, item] : values) {
                const std::optional<RuntimeValueView> field =
                    fields->find(name);
                const std::optional<RuntimeMapView> fieldMap =
                    field ? field->map() : std::nullopt;
                const std::optional<RuntimeValueView> fieldType =
                    fieldMap ? fieldMap->find("type") : std::nullopt;
                if (fieldType) {
                    item = resolveStored(
                        service, item, parseSchema(*fieldType), environment,
                        type.module.empty() ? declaringModule : type.module,
                        path + "." + name, true, false);
                }
            }
        }
        result = RuntimeValue(std::move(values));
    }
    result =
        service.constructTypedValue(result, schemaValue(type), declaringModule);
    if (!matchesRuntime(result, type, declaringModule)) {
        valueTypeError(path, type);
    }
    return result;
}

RuntimeValue resolveStored(const TypedDataService& service,
                           const RuntimeValue& value, const TypeSchema& type,
                           const RuntimeValue::Map& environment,
                           const std::string& declaringModule,
                           const std::string& path, bool strict,
                           bool evaluateAnyExpressions) {
    if (value.isNil()) {
        if (strict && type.kind == TypeSchema::Kind::Union) {
            throw std::invalid_argument(
                path + ": union value requires $type and $value");
        }
        if (strict && type.kind != TypeSchema::Kind::Optional &&
            !matchesRuntime(value, type, declaringModule)) {
            valueTypeError(path, type);
        }
        return value;
    }
    if (type.kind == TypeSchema::Kind::Optional) {
        return resolveStored(service, value, type.arguments.front(),
                             environment, declaringModule, path, strict,
                             evaluateAnyExpressions);
    }
    if (type.kind == TypeSchema::Kind::Union) {
        const RuntimeValue owned = snapshotContainer(value);
        const std::optional<RuntimeMapView> wrapper = owned.view().map();
        const std::optional<RuntimeValueView> branch =
            wrapper ? wrapper->find("$type") : std::nullopt;
        if (!wrapper || !branch || wrapper->size() > 2) {
            throw std::invalid_argument(
                path + ": union value requires $type and $value");
        }
        const TypeSchema selected = parseSchema(*branch);
        if (std::find(type.arguments.begin(), type.arguments.end(), selected) ==
            type.arguments.end()) {
            throw std::invalid_argument(path + ": undeclared union branch " +
                                        schemaName(selected));
        }
        const std::optional<RuntimeValueView> data = wrapper->find("$value");
        const bool nilBranch = selected.kind == TypeSchema::Kind::Named &&
                               selected.module.empty() &&
                               selected.name == "nil";
        if (!data && !nilBranch) {
            throw std::invalid_argument(path +
                                        ": union value is missing $value");
        }
        const RuntimeValue result =
            resolveStored(service, data ? data->toValue() : RuntimeValue(),
                          selected, environment, declaringModule,
                          path + ".$value", true, evaluateAnyExpressions);
        if (!matchesRuntime(result, selected, declaringModule)) {
            valueTypeError(path, selected);
        }
        return result;
    }
    if (type.kind == TypeSchema::Kind::Named && type.module.empty() &&
        type.name == "any") {
        return evaluateAnyExpressions
                   ? service.evalDataExpression(value, environment)
                   : value;
    }
    RuntimeValue resolved = snapshotContainer(value);
    if (type.kind == TypeSchema::Kind::List ||
        type.kind == TypeSchema::Kind::Tuple) {
        if (resolved.getIf<std::string>() != nullptr) {
            resolved = snapshotContainer(
                service.evalDataExpression(resolved, environment));
        }
        const std::optional<RuntimeValue::Array> array =
            ludork::runtime::reference::arrayValues(resolved);
        if (!array || (type.kind == TypeSchema::Kind::Tuple &&
                       array->size() != type.arguments.size())) {
            valueTypeError(path, type);
        }
        RuntimeValue::Array result;
        for (std::size_t index = 0; index < array->size(); ++index) {
            const TypeSchema& itemType = type.kind == TypeSchema::Kind::List
                                             ? type.arguments.front()
                                             : type.arguments[index];
            result.push_back(resolveStored(
                service, (*array)[index], itemType, environment,
                declaringModule, path + "[" + std::to_string(index + 1) + "]",
                strict, false));
        }
        return RuntimeValue(std::move(result));
    }
    if (type.kind == TypeSchema::Kind::Dictionary) {
        if (resolved.getIf<std::string>() != nullptr) {
            resolved = snapshotContainer(
                service.evalDataExpression(resolved, environment));
        }
        const std::optional<RuntimeValue::Map> map =
            ludork::runtime::reference::mapValues(resolved);
        if (!map) {
            valueTypeError(path, type);
        }
        RuntimeValue::Map result;
        for (const auto& [key, item] : *map) {
            result.emplace(key,
                           resolveStored(service, item, type.arguments.front(),
                                         environment, declaringModule,
                                         path + "." + key, strict, false));
        }
        return RuntimeValue(std::move(result));
    }
    if (type.module.empty() && (type.name == "string" || type.name == "file")) {
        if (strict && resolved.getIf<std::string>() == nullptr) {
            valueTypeError(path, type);
        }
        const RuntimeValue scalarType(type.name);
        return type.name == "file"
                   ? resolved
                   : service.coerceStandardValue(resolved, scalarType);
    }
    if (type.module.empty() && (type.name == "int" || type.name == "float" ||
                                type.name == "bool" || type.name == "nil")) {
        if (strict && !matchesRuntime(resolved, type, declaringModule)) {
            valueTypeError(path, type);
        }
        const RuntimeValue scalarType(type.name);
        resolved = service.coerceStandardValue(resolved, scalarType);
        if (!matchesRuntime(resolved, type, declaringModule)) {
            valueTypeError(path, type);
        }
        return resolved;
    }
    if (type.module.empty() &&
        (type.name == "table" || type.name == "Pair" || type.name == "pair")) {
        resolved = service.evalDataExpression(resolved, environment);
        if (!matchesRuntime(resolved, type, declaringModule)) {
            valueTypeError(path, type);
        }
        return resolved;
    }
    if (matchesRuntime(value, type, declaringModule)) {
        return value;
    }
    if (resolved.getIf<std::string>() != nullptr) {
        resolved = service.evalDataExpression(resolved, environment);
        if (matchesRuntime(resolved, type, declaringModule)) {
            return resolved;
        }
    }
    if (type.module.empty() &&
        (type.name == "function" || type.name == "event")) {
        valueTypeError(path, type);
    }
    return resolveStoredRecord(service, resolved, type, environment,
                               declaringModule, path);
}

}  // namespace

bool TypedDataService::isContainerValueType(RuntimeValueView valueType) const {
    return ludork::runtime::typed_data_impl::isContainerSchema(
        parseSchema(valueType));
}

bool TypedDataService::isStandardValueType(RuntimeValueView valueType) const {
    return ludork::runtime::typed_data_impl::isStandardSchema(
        parseSchema(valueType));
}

bool TypedDataService::shouldEvalValueType(RuntimeValueView valueType) const {
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
    const RuntimeValue& value, RuntimeValueView valueType) const {
    if (value.isNil()) {
        return value;
    }
    const RuntimeValueView unwrapped = unwrapOptional(valueType);
    const TypeSchema schema = parseSchema(unwrapped);
    if (schema.kind != TypeSchema::Kind::Named) {
        return resolveStored(*this, value, schema, {}, {}, "value", false);
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
    RuntimeValueView typeReference) const {
    return schemaName(parseSchema(typeReference));
}

RuntimeValue TypedDataService::constructTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule) const {
    return metadataRuntime().constructTypedValue(value, valueType,
                                                 declaringModule);
}

RuntimeValue TypedDataService::resolveTypedDataValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const RuntimeValue::Map& environment, const std::string& declaringModule,
    bool evaluateAnyExpressions) const {
    return resolveStored(*this, value, parseSchema(valueType), environment,
                         declaringModule, "value", true,
                         evaluateAnyExpressions);
}

RuntimeValue TypedDataService::resolveRuntimeTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule) const {
    const TypeSchema schema = parseSchema(valueType);
    if ((!value.isNil() || schema.kind == TypeSchema::Kind::Union) &&
        !matchesRuntime(value, schema, declaringModule)) {
        valueTypeError("runtime value", schema);
    }
    return value;
}

RuntimeValueView TypedDataService::unwrapOptional(
    RuntimeValueView valueType) const {
    const auto optional = mapValue(valueType, "optional");
    return !optional ? valueType : *optional;
}

RuntimeValue TypedDataService::coerceBool(const RuntimeValue& value) const {
    return value;
}

RuntimeValue TypedDataService::coerceInteger(const RuntimeValue& value) const {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return RuntimeValue(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        const double truncated = std::trunc(*number);
        const double limit = std::ldexp(1.0, 63);
        if (!std::isfinite(truncated) || truncated < -limit ||
            truncated >= limit) {
            throw std::invalid_argument(
                "Integer conversion is outside the signed 64-bit range");
        }
        return RuntimeValue(static_cast<std::int64_t>(truncated));
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
    const RuntimeValue::Map& environment, const std::string& declaringModule,
    bool evaluateAnyExpressions) {
    return typedDataService().resolveTypedDataValue(
        value, valueType, environment, declaringModule, evaluateAnyExpressions);
}

RuntimeValue::Map getConfigVars(const RuntimeValue& meta) {
    return metadataRuntime().configVars(meta);
}

RuntimeValue dataValueResolveRuntimeTypedValue(
    const RuntimeValue& value, const RuntimeValue& valueType,
    const std::string& declaringModule) {
    return typedDataService().resolveRuntimeTypedValue(value, valueType,
                                                       declaringModule);
}

void dataValueSetRuntimeTypedAttribute(const RuntimeValue& owner,
                                       const std::string& name,
                                       const RuntimeValue& value) {
    TypedDataService& service = typedDataService();
    const RuntimeValue ownerType = runtimeReflection().typeOf(owner);
    const RuntimeValue metadata = service.resolveAttrMetadata(ownerType, name);
    const std::optional<RuntimeValueView> type = mapValue(metadata, "type");
    const std::optional<RuntimeValueView> module = mapValue(metadata, "module");
    const std::string* declaringModule =
        module ? module->getIf<std::string>() : nullptr;
    const RuntimeValue valueType =
        type ? type->toValue() : service.resolveAttrValueType(ownerType, name);
    const RuntimeValue resolved = service.resolveRuntimeTypedValue(
        value, valueType,
        declaringModule == nullptr ? std::string() : *declaringModule);
    runtimeReflection().setTyped(ludork::runtime::reference::intern(owner),
                                 name, resolved);
}
