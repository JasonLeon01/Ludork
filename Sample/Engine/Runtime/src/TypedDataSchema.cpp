#include "TypedDataSchema.hpp"
#include <Runtime/RuntimeReference.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ludork::runtime::typed_data_impl {
namespace {

std::string trim(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char item) {
                       return static_cast<char>(std::tolower(item));
                   });
    return value;
}

TypeSchema named(std::string name, std::string module = {}) {
    if (name.ends_with("[]")) {
        TypeSchema type;
        type.kind = TypeSchema::Kind::List;
        type.arguments.push_back(
            named(name.substr(0, name.size() - 2), std::move(module)));
        return type;
    }
    const std::size_t separator = name.find_last_of('.');
    if (module.empty() && separator != std::string::npos) {
        module = name.substr(0, separator);
        name = name.substr(separator + 1);
    }
    if (module.empty()) {
        const std::string normalized = lower(name);
        if (normalized == "integer") {
            name = "int";
        } else if (normalized == "double" || normalized == "number") {
            name = "float";
        } else if (normalized == "boolean") {
            name = "bool";
        } else if (normalized == "nil" || normalized == "any" ||
                   normalized == "int" || normalized == "float" ||
                   normalized == "bool" || normalized == "string" ||
                   normalized == "file" || normalized == "function" ||
                   normalized == "event" || normalized == "table") {
            name = normalized;
        }
    }
    TypeSchema type;
    type.name = std::move(name);
    type.module = std::move(module);
    return type;
}

TypeSchema composite(TypeSchema::Kind kind, std::vector<TypeSchema> arguments) {
    if (arguments.empty()) {
        throw std::invalid_argument(
            "Metadata composite type requires arguments");
    }
    TypeSchema type;
    type.kind = kind;
    if (kind == TypeSchema::Kind::Union) {
        for (TypeSchema& argument : arguments) {
            std::vector<TypeSchema> items;
            if (argument.kind == TypeSchema::Kind::Union) {
                items = std::move(argument.arguments);
            } else {
                items.push_back(std::move(argument));
            }
            for (TypeSchema& item : items) {
                if (std::find(type.arguments.begin(), type.arguments.end(),
                              item) == type.arguments.end()) {
                    type.arguments.push_back(std::move(item));
                }
            }
        }
    } else {
        type.arguments = std::move(arguments);
    }
    return type;
}

TypeSchema parseText(std::string text) {
    text = trim(std::move(text));
    if (text.empty()) {
        throw std::invalid_argument("Metadata type name must not be empty");
    }
    if (text.ends_with("[]")) {
        return composite(TypeSchema::Kind::List,
                         {parseText(text.substr(0, text.size() - 2))});
    }
    const std::string normalized = lower(text);
    if (normalized == "list" || normalized == "array") {
        return composite(TypeSchema::Kind::List, {named("any")});
    }
    if (normalized == "dict" || normalized == "dictionary" ||
        normalized == "map") {
        return composite(TypeSchema::Kind::Dictionary, {named("any")});
    }
    const std::size_t opening = text.find('[');
    if (opening == std::string::npos || text.back() != ']') {
        return named(std::move(text));
    }
    std::vector<TypeSchema> arguments;
    std::size_t begin = opening + 1;
    int depth = 0;
    for (std::size_t index = begin; index < text.size() - 1; ++index) {
        if (text[index] == '[') {
            ++depth;
        } else if (text[index] == ']') {
            if (--depth < 0) {
                throw std::invalid_argument("Invalid metadata type: " + text);
            }
        } else if (text[index] == ',' && depth == 0) {
            arguments.push_back(parseText(text.substr(begin, index - begin)));
            begin = index + 1;
        }
    }
    if (depth != 0) {
        throw std::invalid_argument("Invalid metadata type: " + text);
    }
    arguments.push_back(parseText(text.substr(begin, text.size() - 1 - begin)));
    const std::string container = lower(trim(text.substr(0, opening)));
    if ((container == "list" || container == "array" || container == "set") &&
        arguments.size() == 1) {
        return composite(TypeSchema::Kind::List, std::move(arguments));
    }
    if ((container == "dict" || container == "dictionary" ||
         container == "map") &&
        arguments.size() == 2 && arguments[0] == named("string")) {
        return composite(TypeSchema::Kind::Dictionary,
                         {std::move(arguments[1])});
    }
    if (container == "tuple") {
        return composite(TypeSchema::Kind::Tuple, std::move(arguments));
    }
    if (container == "union") {
        return composite(TypeSchema::Kind::Union, std::move(arguments));
    }
    if (container == "optional" && arguments.size() == 1) {
        return composite(TypeSchema::Kind::Optional, std::move(arguments));
    }
    throw std::invalid_argument("Invalid metadata type: " + text);
}

}  // namespace

TypeSchema parseSchema(RuntimeValueView value) {
    if (value.getIf<RuntimeHandle>() != nullptr) {
        const RuntimeValue snapshot =
            ludork::runtime::reference::snapshot(value.toValue());
        if (snapshot.getIf<RuntimeHandle>() != nullptr) {
            throw std::invalid_argument("Metadata schema must be pure data");
        }
        return parseSchema(snapshot);
    }
    if (value.isNil()) {
        return named("any");
    }
    if (const std::string* name = value.getIf<std::string>()) {
        return parseText(*name);
    }
    if (const std::optional<RuntimeArrayView> reference = value.array()) {
        if (reference->size() == 2) {
            const std::string* module = (*reference)[0].getIf<std::string>();
            const std::string* name = (*reference)[1].getIf<std::string>();
            if (module != nullptr && name != nullptr && !module->empty() &&
                !name->empty()) {
                return named(*name, *module);
            }
        }
        throw std::invalid_argument(
            "Metadata type reference requires module and type names");
    }
    const std::optional<RuntimeMapView> map = value.map();
    if (!map || map->size() != 1) {
        throw std::invalid_argument(
            "Metadata type must be a name, module reference or composite "
            "schema");
    }
    for (const auto& [key, argument] : *map) {
        if (key == "optional" || key == "list" || key == "dict") {
            const TypeSchema::Kind kind =
                key == "optional" ? TypeSchema::Kind::Optional
                : key == "list"   ? TypeSchema::Kind::List
                                  : TypeSchema::Kind::Dictionary;
            return composite(kind, {parseSchema(argument)});
        }
        if (key == "union" || key == "tuple") {
            const std::optional<RuntimeArrayView> items = argument.array();
            if (!items || items->empty()) {
                throw std::invalid_argument("Metadata " + key +
                                            " requires an ordered type array");
            }
            std::vector<TypeSchema> arguments;
            for (RuntimeValueView item : *items) {
                arguments.push_back(parseSchema(item));
            }
            return composite(key == "union" ? TypeSchema::Kind::Union
                                            : TypeSchema::Kind::Tuple,
                             std::move(arguments));
        }
    }
    throw std::invalid_argument("Unknown metadata composite type");
}

RuntimeValue schemaValue(const TypeSchema& type) {
    if (type.kind == TypeSchema::Kind::Named) {
        if (type.module.empty()) {
            return RuntimeValue(type.name);
        }
        if (type.module == "sf") {
            return RuntimeValue(type.module + "." + type.name);
        }
        return RuntimeValue(RuntimeValue::Array{RuntimeValue(type.module),
                                                RuntimeValue(type.name)});
    }
    if (type.kind == TypeSchema::Kind::Optional ||
        type.kind == TypeSchema::Kind::List ||
        type.kind == TypeSchema::Kind::Dictionary) {
        const char* key = type.kind == TypeSchema::Kind::Optional ? "optional"
                          : type.kind == TypeSchema::Kind::List   ? "list"
                                                                  : "dict";
        return RuntimeValue(
            RuntimeValue::Map{{key, schemaValue(type.arguments.front())}});
    }
    RuntimeValue::Array arguments;
    for (const TypeSchema& argument : type.arguments) {
        arguments.push_back(schemaValue(argument));
    }
    return RuntimeValue(RuntimeValue::Map{
        {type.kind == TypeSchema::Kind::Union ? "union" : "tuple",
         RuntimeValue(std::move(arguments))}});
}

std::string schemaName(const TypeSchema& type) {
    if (type.kind == TypeSchema::Kind::Named) {
        return type.module.empty() ? type.name : type.module + "." + type.name;
    }
    std::string result = type.kind == TypeSchema::Kind::Union   ? "Union["
                         : type.kind == TypeSchema::Kind::Tuple ? "Tuple["
                         : type.kind == TypeSchema::Kind::List  ? "List["
                         : type.kind == TypeSchema::Kind::Dictionary
                             ? "Dict[string, "
                             : "Optional[";
    for (std::size_t index = 0; index < type.arguments.size(); ++index) {
        if (index != 0) {
            result += ", ";
        }
        result += schemaName(type.arguments[index]);
    }
    return result + "]";
}

bool isContainerSchema(const TypeSchema& type) {
    return type.kind == TypeSchema::Kind::List ||
           type.kind == TypeSchema::Kind::Dictionary ||
           type.kind == TypeSchema::Kind::Tuple ||
           (type.kind == TypeSchema::Kind::Named && type.module.empty() &&
            (type.name == "table" || type.name == "Pair" ||
             type.name == "pair"));
}

bool isStandardSchema(const TypeSchema& type) {
    if (type.kind != TypeSchema::Kind::Named) {
        return std::all_of(type.arguments.begin(), type.arguments.end(),
                           isStandardSchema);
    }
    return type.module.empty() &&
           (type.name == "any" || type.name == "nil" || type.name == "bool" ||
            type.name == "int" || type.name == "float" ||
            type.name == "string" || type.name == "file" ||
            type.name == "table" || type.name == "Pair" || type.name == "pair");
}

}  // namespace ludork::runtime::typed_data_impl
