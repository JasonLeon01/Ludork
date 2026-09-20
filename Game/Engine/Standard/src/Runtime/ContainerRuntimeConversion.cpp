#include "ContainerRuntimeInternal.hpp"
#include <JsonRuntimeProtocol.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

struct TableConversionContext {
    explicit TableConversionContext(lua_glue::StateView state) : lua(state) {}

    lua_glue::StateView lua;
    std::unordered_map<const void*, lua_glue::Table> converted;
};

std::string luaStringValue(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push(state);
    std::size_t length = 0;
    const char* raw = luaL_tolstring(state, -1, &length);
    std::string result(raw, length);
    lua_pop(state, 2);
    return result;
}

std::string tupleNumberString(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push(state);
    int integerValid = 0;
    const lua_Integer integer = lua_tointegerx(state, -1, &integerValid);
    lua_pop(state, 1);
    if (integerValid != 0) {
        return std::to_string(static_cast<long long>(integer));
    }
    return luaStringValue(value);
}

std::string quotedString(std::string_view value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (character < 0x20U || character == 0x7fU) {
                    result += "\\x";
                    result.push_back(digits[character >> 4U]);
                    result.push_back(digits[character & 0x0fU]);
                } else {
                    result.push_back(static_cast<char>(character));
                }
                break;
        }
    }
    result.push_back('"');
    return result;
}

std::string tupleItemString(const lua_glue::Object& value);

std::string referenceString(const lua_glue::Object& value) {
    const char* typeName = "reference";
    switch (value.get_type()) {
        case lua_glue::Type::Table:
            typeName = "table";
            break;
        case lua_glue::Type::Function:
            typeName = "function";
            break;
        case lua_glue::Type::Thread:
            typeName = "thread";
            break;
        case lua_glue::Type::Userdata:
            typeName = "userdata";
            break;
        case lua_glue::Type::LightUserdata:
            typeName = "lightuserdata";
            break;
        default:
            break;
    }
    char address[2 + sizeof(void*) * 2 + 1] = {};
    std::snprintf(address, sizeof(address), "%p", objectIdentity(value));
    return "<" + std::string(typeName) + ":" + address + ">";
}

std::string tupleString(const lua_glue::Object& value) {
    const NativeTuple& tuple = value.as<NativeTuple&>();
    const lua_glue::Table values = sequenceValues(value);
    std::string result = "(";
    for (std::size_t index = 1; index <= tuple.length; ++index) {
        if (index > 1) {
            result.push_back(',');
        }
        result += tupleItemString(values.raw_get<lua_glue::Object>(index));
    }
    if (tuple.length == 1) {
        result.push_back(',');
    }
    result.push_back(')');
    return result;
}

std::string tupleItemString(const lua_glue::Object& value) {
    switch (value.get_type()) {
        case lua_glue::Type::Boolean:
            return value.as<bool>() ? "true" : "false";
        case lua_glue::Type::Number:
            return tupleNumberString(value);
        case lua_glue::Type::String:
            return quotedString(value.as<std::string_view>());
        case lua_glue::Type::Userdata:
            if (value.is<NativeTuple>()) {
                return tupleString(value);
            }
            [[fallthrough]];
        case lua_glue::Type::Table:
        case lua_glue::Type::Function:
        case lua_glue::Type::Thread:
        case lua_glue::Type::LightUserdata:
            return referenceString(value);
        case lua_glue::Type::Nil:
        case lua_glue::Type::None:
            throw std::invalid_argument("tuple elements cannot be nil");
        default:
            return luaStringValue(value);
    }
}

lua_glue::Object convertToTable(const lua_glue::Object& value,
                                TableConversionContext& context);

lua_glue::Object convertedSequence(const lua_glue::Object& value,
                                   TableConversionContext& context,
                                   std::size_t length) {
    const void* identity = objectIdentity(value);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return lua_glue::MakeObject(context.lua, existing->second);
    }
    lua_glue::Table result =
        context.lua.create_table(static_cast<int>(length), 0);
    const lua_glue::Object arrayMetatable =
        context.lua.registry().raw_get<lua_glue::Object>(
            ludork::standard::json_runtime::protocol::JSON_ARRAY_METATABLE_KEY);
    if (arrayMetatable.get_type() != lua_glue::Type::Table) {
        throw std::runtime_error("cjson array metatable is not registered");
    }
    lua_glue::SetMetatable(result, arrayMetatable.as<lua_glue::Table>());
    context.converted.emplace(identity, result);
    const lua_glue::Table values = sequenceValues(value);
    for (std::size_t index = 1; index <= length; ++index) {
        result.raw_set(
            index, convertToTable(
                       exposedValue(context.lua,
                                    values.raw_get<lua_glue::Object>(index)),
                       context));
    }
    return lua_glue::MakeObject(context.lua, result);
}

lua_glue::Object convertedDict(const lua_glue::Object& value,
                               TableConversionContext& context) {
    const void* identity = objectIdentity(value);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return lua_glue::MakeObject(context.lua, existing->second);
    }
    lua_glue::Table result = context.lua.create_table();
    context.converted.emplace(identity, result);
    const NativeDict& dict = value.as<NativeDict&>();
    const lua_glue::Table keys = dictKeys(value);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        const lua_glue::Object sourceKey =
            keys.raw_get<lua_glue::Object>(index + 1);
        const lua_glue::Object targetKey =
            sourceKey.is<NativeTuple>()
                ? lua_glue::MakeObject(context.lua, tupleString(sourceKey))
                : convertToTable(sourceKey, context);
        const lua_glue::Object occupied =
            result.raw_get<lua_glue::Object>(targetKey);
        if (occupied.valid() && occupied.get_type() != lua_glue::Type::Nil) {
            throw std::invalid_argument(
                "dict.toTable key conversion would overwrite an existing key");
        }
        result.raw_set(
            targetKey,
            convertToTable(dictEntryValue(context.lua, value, index), context));
    }
    return lua_glue::MakeObject(context.lua, result);
}

lua_glue::Object convertedRawTable(const lua_glue::Table& value,
                                   TableConversionContext& context) {
    const lua_glue::Object source = lua_glue::MakeObject(context.lua, value);
    const void* identity = objectIdentity(source);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return lua_glue::MakeObject(context.lua, existing->second);
    }
    lua_glue::Table result = context.lua.create_table();
    const lua_glue::Object arrayMetatable =
        context.lua.registry().raw_get<lua_glue::Object>(
            ludork::standard::json_runtime::protocol::JSON_ARRAY_METATABLE_KEY);
    if (arrayMetatable.get_type() != lua_glue::Type::Table) {
        throw std::runtime_error("cjson array metatable is not registered");
    }
    const lua_glue::Object emptyArrayMetatable =
        context.lua.registry().raw_get<lua_glue::Object>(
            ludork::standard::json_runtime::protocol::
                JSON_EMPTY_ARRAY_METATABLE_KEY);
    if (emptyArrayMetatable.get_type() != lua_glue::Type::Table) {
        throw std::runtime_error(
            "cjson empty-array metatable is not registered");
    }
    lua_State* state = context.lua.lua_state();
    const int originalTop = lua_gettop(state);
    value.push(state);
    const bool hasMetatable = lua_getmetatable(state, -1) != 0;
    bool isJsonArray = false;
    bool isJsonEmptyArray = false;
    if (hasMetatable) {
        arrayMetatable.push(state);
        isJsonArray = lua_rawequal(state, -1, -2) != 0;
        lua_pop(state, 1);
        emptyArrayMetatable.push(state);
        isJsonEmptyArray = lua_rawequal(state, -1, -2) != 0;
    }
    lua_settop(state, originalTop);
    if (isJsonArray) {
        lua_glue::SetMetatable(result, arrayMetatable.as<lua_glue::Table>());
    } else if (isJsonEmptyArray) {
        lua_glue::SetMetatable(result,
                               emptyArrayMetatable.as<lua_glue::Table>());
    }
    context.converted.emplace(identity, result);
    for (const auto& entry : value) {
        result.raw_set(convertToTable(entry.first, context),
                       convertToTable(entry.second, context));
    }
    return lua_glue::MakeObject(context.lua, result);
}

lua_glue::Object convertToTable(const lua_glue::Object& value,
                                TableConversionContext& context) {
    if (value.get_type() == lua_glue::Type::Nil) {
        const lua_glue::Object nullValue =
            context.lua.registry().raw_get<lua_glue::Object>(
                ludork::standard::json_runtime::protocol::JSON_NULL_KEY);
        if (!nullValue.valid() || nullValue.get_type() == lua_glue::Type::Nil) {
            throw std::runtime_error("cjson.null is not registered");
        }
        return nullValue;
    }
    if (value.is<NativeList>()) {
        return convertedSequence(value, context,
                                 value.as<NativeList&>().length);
    }
    if (value.is<NativeTuple>()) {
        return convertedSequence(value, context,
                                 value.as<NativeTuple&>().length);
    }
    if (value.is<NativeDict>()) {
        return convertedDict(value, context);
    }
    if (value.get_type() == lua_glue::Type::Table) {
        return convertedRawTable(value.as<lua_glue::Table>(), context);
    }
    return value;
}

lua_glue::Object containerToTable(const lua_glue::Object& value,
                                  lua_glue::ThisState state) {
    TableConversionContext context{lua_glue::StateView(state)};
    return convertToTable(value, context);
}

}  // namespace ludork::standard::container_runtime::detail
